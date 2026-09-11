#!/usr/bin/env python3
import argparse
import csv
import struct
from collections import Counter
from pathlib import Path

FNV_OFFSET = 14695981039346656037
FNV_PRIME = 1099511628211


def fnv1a64(data: bytes) -> int:
    h = FNV_OFFSET
    for b in data:
        h ^= b
        h = (h * FNV_PRIME) & 0xFFFFFFFFFFFFFFFF
    return h


def cstr(blob: bytes, start: int) -> str:
    if start < 0 or start >= len(blob):
        return ""
    end = blob.find(b"\0", start)
    if end < 0:
        end = len(blob)
    return blob[start:end].decode("ascii", "replace")


def parse_ctab(shader: bytes):
    idx = shader.find(b"CTAB")
    if idx < 0 or idx + 32 > len(shader):
        return []
    base = idx + 4
    try:
        size, creator_off, version, count, info_off, flags, target_off = struct.unpack_from("<7I", shader, base)
    except struct.error:
        return []
    if size < 28 or count > 1024:
        return []
    out = []
    for i in range(count):
        pos = base + info_off + i * 20
        if pos + 20 > len(shader):
            break
        name_off, regset, regindex, regcount, reserved, type_off, default_off = struct.unpack_from("<IHHHHII", shader, pos)
        out.append({
            "name": cstr(shader, base + name_off),
            "regset": regset,
            "regindex": regindex,
            "regcount": regcount,
        })
    return out


def parse_shader_package(path: Path):
    data = path.read_bytes()
    if len(data) < 12:
        raise ValueError("Shader package is too short")
    package_id, declared_count, payload_size = struct.unpack_from("<III", data, 0)
    pos = 12
    shaders = {}
    records = 0
    while pos + 260 <= len(data):
        name = data[pos:pos + 256].split(b"\0", 1)[0].decode("ascii", "replace")
        size = struct.unpack_from("<I", data, pos + 256)[0]
        start = pos + 260
        end = start + size
        if size <= 0 or end > len(data):
            break
        code = data[start:end]
        h = fnv1a64(code)
        if h not in shaders:
            shaders[h] = {
                "name": name,
                "names": [name],
                "size": size,
                "ctab": parse_ctab(code),
            }
        else:
            shaders[h]["names"].append(name)
        records += 1
        pos = end
    if declared_count and records != declared_count:
        print(f"warning: package declares {declared_count} shaders but parsed {records}")
    return package_id, records, shaders


def kv_tokens(line: str):
    out = {}
    for token in line.strip().split()[1:]:
        if "=" in token:
            k, v = token.split("=", 1)
            out[k] = v
    return out


def parse_capture(path: Path):
    draws = {}
    frame_ids = []
    with path.open("r", encoding="utf-8", errors="replace") as f:
        for raw in f:
            line = raw.strip()
            if line.startswith("FRAME_BEGIN "):
                kv = kv_tokens(line)
                if "id" in kv:
                    frame_ids.append(int(kv["id"]))
            elif line.startswith("DRAW "):
                kv = kv_tokens(line)
                draw_id = int(kv["id"])
                draws[draw_id] = {
                    "meta": kv,
                    "tex": {},
                    "psf": {},
                    "vsf": {},
                    "rs": {},
                    "ss": {},
                }
            elif line.startswith("TEX "):
                kv = kv_tokens(line)
                draw_id = int(kv["draw"])
                stage = int(kv["stage"])
                if draw_id in draws:
                    draws[draw_id]["tex"][stage] = kv
            elif line.startswith("PSF ") or line.startswith("VSF "):
                kind = line[:3].lower()
                kv = kv_tokens(line)
                draw_id = int(kv["draw"])
                reg = int(kv["r"])
                vals = tuple(float(x) for x in kv["v"].split(","))
                if draw_id in draws:
                    draws[draw_id][kind][reg] = vals
            elif line.startswith("RS "):
                kv = kv_tokens(line)
                draw_id = int(kv["draw"])
                if draw_id in draws:
                    draws[draw_id]["rs"][kv["name"]] = int(kv["value"])
            elif line.startswith("SS "):
                kv = kv_tokens(line)
                draw_id = int(kv["draw"])
                sampler = int(kv["sampler"])
                if draw_id in draws:
                    draws[draw_id]["ss"][(sampler, kv["name"])] = int(kv["value"])
    return frame_ids, draws


def fmt_vec(v):
    if v is None:
        return ""
    return ",".join(f"{x:.9g}" for x in v)


def texture_summary(tex):
    if not tex:
        return ""
    pieces = []
    for key in ("w", "h", "fmt", "hash", "bytes", "ptr"):
        if key in tex:
            pieces.append(f"{key}={tex[key]}")
    return " ".join(pieces)


def constant_lookup(shader_info, draw, stage):
    result = {}
    if not shader_info:
        return result
    float_regs = draw["psf"] if stage == "ps" else draw["vsf"]
    for c in shader_info["ctab"]:
        if c["regset"] == 2:
            result[c["name"]] = float_regs.get(c["regindex"], (0.0, 0.0, 0.0, 0.0))
        elif c["regset"] == 3:
            result[c["name"]] = c["regindex"]
    return result


def main():
    ap = argparse.ArgumentParser(description="Map Fallout3D3D9.log shader hashes to Fallout 3 SDP shader names and extract PPLighting draws.")
    ap.add_argument("shader_package", type=Path, help="Path to shaderpackage017.sdp")
    ap.add_argument("capture_log", type=Path, help="Path to Fallout3D3D9.log")
    ap.add_argument("--out-dir", type=Path, default=None, help="Output directory (default: next to capture log)")
    args = ap.parse_args()

    out_dir = args.out_dir or args.capture_log.parent
    out_dir.mkdir(parents=True, exist_ok=True)

    package_id, shader_records, shaders = parse_shader_package(args.shader_package)
    frames, draws = parse_capture(args.capture_log)

    ps_counts = Counter()
    vs_counts = Counter()
    pair_counts = Counter()
    for draw in draws.values():
        ps = int(draw["meta"].get("ps", "0"), 16)
        vs = int(draw["meta"].get("vs", "0"), 16)
        ps_counts[ps] += 1
        vs_counts[vs] += 1
        pair_counts[(ps, vs)] += 1

    summary_path = out_dir / "Fallout3D3D9-summary.txt"
    with summary_path.open("w", encoding="utf-8") as out:
        out.write(f"shaderPackageId={package_id}\n")
        out.write(f"shaderRecords={shader_records}\n")
        out.write(f"uniqueShaderBytecodes={len(shaders)}\n")
        out.write(f"capturedFrames={frames}\n")
        out.write(f"draws={len(draws)}\n")
        out.write(f"uniquePixelShaders={len(ps_counts)}\n")
        out.write(f"uniqueVertexShaders={len(vs_counts)}\n\n")
        out.write("Top shader pairs:\n")
        for (ps, vs), count in pair_counts.most_common(50):
            ps_entry = shaders.get(ps)
            vs_entry = shaders.get(vs)
            ps_name = "|".join(ps_entry["names"]) if ps_entry else "UNMAPPED"
            vs_name = "|".join(vs_entry["names"]) if vs_entry else "UNMAPPED"
            out.write(f"{count:5d}  PS {ps:016x} {ps_name:20s} | VS {vs:016x} {vs_name}\n")

    csv_path = out_dir / "Fallout3D3D9-pplighting.csv"
    fieldnames = [
        "draw", "frame", "primitive", "primitiveCount", "numVertices",
        "pixelShader", "vertexShader", "AmbientColor", "PSLightColor",
        "BaseMapSampler", "BaseMapTexture", "BaseMapSRGB",
        "NormalMapSampler", "NormalMapTexture", "NormalMapSRGB",
        "SRGBWRITEENABLE", "CULLMODE", "ALPHABLENDENABLE",
    ]
    rows = 0
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for draw_id in sorted(draws):
            draw = draws[draw_id]
            ps_hash = int(draw["meta"].get("ps", "0"), 16)
            vs_hash = int(draw["meta"].get("vs", "0"), 16)
            ps_info = shaders.get(ps_hash)
            vs_info = shaders.get(vs_hash)
            if not ps_info:
                continue
            names = {c["name"] for c in ps_info["ctab"]}
            if not {"AmbientColor", "PSLightColor", "BaseMap"}.issubset(names):
                continue
            constants = constant_lookup(ps_info, draw, "ps")
            base_sampler = constants.get("BaseMap")
            normal_sampler = constants.get("NormalMap")
            writer.writerow({
                "draw": draw_id,
                "frame": draw["meta"].get("frame", ""),
                "primitive": draw["meta"].get("primitive", ""),
                "primitiveCount": draw["meta"].get("primitiveCount", ""),
                "numVertices": draw["meta"].get("numVertices", ""),
                "pixelShader": "|".join(ps_info["names"]),
                "vertexShader": "|".join(vs_info["names"]) if vs_info else "UNMAPPED",
                "AmbientColor": fmt_vec(constants.get("AmbientColor")),
                "PSLightColor": fmt_vec(constants.get("PSLightColor")),
                "BaseMapSampler": base_sampler if isinstance(base_sampler, int) else "",
                "BaseMapTexture": texture_summary(draw["tex"].get(base_sampler)) if isinstance(base_sampler, int) else "",
                "BaseMapSRGB": draw["ss"].get((base_sampler, "SRGBTEXTURE"), "") if isinstance(base_sampler, int) else "",
                "NormalMapSampler": normal_sampler if isinstance(normal_sampler, int) else "",
                "NormalMapTexture": texture_summary(draw["tex"].get(normal_sampler)) if isinstance(normal_sampler, int) else "",
                "NormalMapSRGB": draw["ss"].get((normal_sampler, "SRGBTEXTURE"), "") if isinstance(normal_sampler, int) else "",
                "SRGBWRITEENABLE": draw["rs"].get("SRGBWRITEENABLE", ""),
                "CULLMODE": draw["rs"].get("CULLMODE", ""),
                "ALPHABLENDENABLE": draw["rs"].get("ALPHABLENDENABLE", ""),
            })
            rows += 1

    print(f"Parsed {shader_records} shader records ({len(shaders)} unique bytecodes) and {len(draws)} draws")
    print(f"Wrote {summary_path}")
    print(f"Wrote {csv_path} ({rows} PPLighting draws)")


if __name__ == "__main__":
    main()
