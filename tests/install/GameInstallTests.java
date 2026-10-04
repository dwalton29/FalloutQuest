package com.falloutquest.app;
import java.io.File;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Comparator;

public final class GameInstallTests {
    static void require(boolean value) { if (!value) throw new AssertionError(); }
    static File fixture(Path data, String master) throws Exception {
        Files.createDirectories(data);
        for (String name : new String[]{master, "Fallout - Meshes.bsa",
                "Fallout - Textures.bsa", "Fallout - Misc.bsa"})
            Files.write(data.resolve(name), new byte[]{1});
        return data.toFile();
    }
    public static void main(String[] args) throws Exception {
        Path temp = Files.createTempDirectory("fq-install");
        try {
            File fo3 = temp.resolve("Fallout3").toFile();
            require(GameInstall.findData(fo3, GameInstall.Game.FALLOUT3) == null);
            File nested = fixture(temp.resolve("Fallout3/Fallout 3 goty/data"), "fallout3.ESM");
            require(nested.equals(GameInstall.findData(fo3, GameInstall.Game.FALLOUT3)));
            require(GameInstall.missing(nested, GameInstall.Game.FALLOUT3).isEmpty());
            require(GameInstall.findData(fo3, GameInstall.Game.NEW_VEGAS) == null);
            fixture(temp.resolve("Fallout3/Data"), "Fallout3.esm");
            require(GameInstall.findData(fo3, GameInstall.Game.FALLOUT3) == null);
            File nv = fixture(temp.resolve("FalloutNV/Data"), "FalloutNV.esm");
            require(nv.equals(GameInstall.findData(temp.resolve("FalloutNV").toFile(), GameInstall.Game.NEW_VEGAS)));
            Files.delete(nv.toPath().resolve("Fallout - Meshes.bsa"));
            require(GameInstall.missing(nv, GameInstall.Game.NEW_VEGAS).contains("Meshes.bsa"));
            Files.write(nv.toPath().resolve("FalloutNV.esm"), new byte[0]);
            require(GameInstall.findData(temp.resolve("FalloutNV").toFile(), GameInstall.Game.NEW_VEGAS) == null);
            System.out.println("Install discovery, case, validation, ambiguity and game isolation passed");
        } finally {
            try (var paths = Files.walk(temp)) {
                paths.sorted(Comparator.reverseOrder()).forEach(path -> path.toFile().delete());
            }
        }
    }
}
