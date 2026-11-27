"""Dependencies in original SCDA operands and their owning records. Never execute input."""
from collections import Counter, defaultdict
import math
import re
import struct

from audit_esm import first, form, records, values

FLOW = {0x10: 'Begin', 0x11: 'End', 0x15: 'Set', 0x16: 'If', 0x17: 'Else',
        0x18: 'ElseIf', 0x19: 'EndIf', 0x1c: 'Reference', 0x1d: 'ScriptName', 0x1e: 'Return'}
SCRIPT_FIELDS = {'SCDA', 'SCTX', 'SCRO', 'SCRV', 'SLSD', 'SCVR'}
OPERATORS = {'+', '-', '*', '/', '%', '==', '!=', '<', '>', '<=', '>=', '&&', '||', '~', '!'}
NUMBER = re.compile(rb'[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)\Z')


class Reader:
    def __init__(self, data):
        self.data = data
        self.at = 0

    def take(self, size):
        end = self.at + size
        if end > len(self.data):
            raise ValueError('truncated operand')
        value = self.data[self.at:end]
        self.at = end
        return value

    def word(self):
        return int.from_bytes(self.take(2), 'little')

    def left(self):
        return len(self.data) - self.at


def instructions(data, known):
    r = Reader(data)
    while r.left():
        offset = r.at
        if r.left() < 4:
            raise ValueError(f'truncated header at {offset}')
        opcode, size = r.word(), r.word()
        if opcode not in known:
            raise ValueError(f'unknown envelope opcode {opcode:04X} at {offset}')
        if opcode == 0x1c:
            yield offset, opcode, size, b''
        else:
            if size > r.left():
                raise ValueError(f'payload exceeds SCDA at {offset}')
            yield offset, opcode, size, r.take(size)


class Dependencies:
    def __init__(self, native, refs):
        self.native = native
        self.refs = refs
        self.commands = set()
        self.queries = set()
        self.operators = set()
        self.locals = set()
        self.events = []
        self.errors = set()

    def reference(self, r, use):
        index = r.word()
        if index < 1 or index > len(self.refs):
            raise ValueError(f'invalid reference slot {index}')
        self.refs[index - 1]['uses'].add(use)
        return index

    def variable(self, r, tag, use):
        scope = None
        if tag == b'r':
            scope = self.reference(r, use)
            tag = r.take(1)
        if tag in (b'G', b'Z'):
            self.reference(r, 'global' if tag == b'G' else 'form ID value')
        elif tag in (b's', b'f', b'l'):
            self.locals.add((scope or 0, r.word(), tag.decode()))
        else:
            raise ValueError('unknown variable type')

    def arguments(self, op, r):
        spec = self.native[op]
        count = r.word() if r.left() else 0
        if count > len(spec['parameters']):
            raise ValueError('custom or unknown argument layout')
        # Fallout 3 ExtractArgs (0x00517950) and its numeric operand reader.
        for param in spec['parameters'][:count]:
            kind = param['type_id']
            if kind == 0:
                r.take(r.word())
            elif kind in (1, 2, 23):
                tag = r.take(1)
                if tag in (b'n', b'z'):
                    r.take(4 if tag == b'n' else 8)
                else:
                    self.variable(r, tag, 'numeric argument scope')
            elif kind == 8:
                r.take(1)
            elif kind in (5, 10, 18, 28, 32, 41, 51, 52, 55):
                r.take(2)
            elif kind == 22:
                # GetScriptVariable/GetQuestVariable encode the variable name as a string.
                r.take(r.word())
            else:
                if r.take(1) != b'r':
                    raise ValueError('unknown form argument')
                self.reference(r, 'argument')
        if op in (0x1059, 0x11b9) and r.left():
            # Both custom commands append a substitution count and typed numeric values.
            for _ in range(r.word()):
                tag = r.take(1)
                if tag in (b'n', b'z'):
                    r.take(4 if tag == b'n' else 8)
                else:
                    self.variable(r, tag, 'message substitution')
            if op == 0x1059 and r.take(4) != bytes(4):
                raise ValueError('unknown ShowMessage trailer')
        if r.left():
            raise ValueError('unmapped trailing arguments')

    def expression(self, r):
        sub = Reader(r.take(r.word()))
        if r.left():
            raise ValueError('expression length mismatch')
        depth = 0
        while sub.left():
            if sub.take(1) != b' ':
                raise ValueError('invalid expression token boundary')
            tag = sub.take(1)
            if tag == b'r':
                index = self.reference(sub, 'expression scope')
                tag = sub.take(1)
                if tag in (b's', b'f', b'l'):
                    self.locals.add((index, sub.word(), tag.decode()))
                    depth += 1
                    continue
                if tag != b'X':
                    raise ValueError('unknown scoped expression')
            if tag == b'X':
                op = sub.word()
                args = Reader(sub.take(sub.word()))
                if op not in self.native:
                    raise ValueError(f'unknown embedded opcode {op:04X}')
                self.queries.add(self.native[op]['name'])
                try:
                    self.arguments(op, args)
                except ValueError as error:
                    self.errors.add(f"expression:{self.native[op]['name']}:{error}")
            elif tag in (b's', b'f', b'l', b'G', b'Z'):
                self.variable(sub, tag, 'expression')
            else:
                end = sub.data.find(b' ', sub.at)
                token = tag + sub.take((len(sub.data) if end == -1 else end) - sub.at)
                if not NUMBER.fullmatch(token):
                    operator = token.decode('ascii', 'replace')
                    if operator not in OPERATORS:
                        raise ValueError('unknown expression operator or literal')
                    arity = 1 if operator in ('~', '!') else 2
                    if depth < arity:
                        raise ValueError('expression stack underflow')
                    self.operators.add(operator)
                    depth -= arity - 1
                    continue
            depth += 1
        if depth != 1:
            raise ValueError('expression stack imbalance')

    def scan(self, data, known):
        for offset, op, size, payload in instructions(data, known):
            name = FLOW.get(op) or (self.native[op]['name'] if op in self.native else f'flow_{op:04X}')
            self.commands.add(name)
            r = Reader(payload)
            try:
                if op == 0x1c:
                    self.reference(Reader(struct.pack('<H', size)), 'command receiver')
                elif op == 0x10:
                    event = {'offset': offset, 'type': r.word(), 'body_bytes': int.from_bytes(r.take(4), 'little')}
                    self.events.append(event)
                    if r.left():
                        argc = r.word()
                        if argc == 1:
                            tag = r.take(1)
                            if tag == b'r':
                                event['filter_slot'] = self.reference(r, 'event filter')
                            elif tag == b'n':
                                event['filter_number'] = int.from_bytes(r.take(4), 'little', signed=True)
                            else:
                                raise ValueError('unknown event filter')
                        elif argc:
                            raise ValueError('unknown event arguments')
                elif op in (0x16, 0x18):
                    r.word()
                    self.expression(r)
                elif op == 0x15:
                    self.variable(r, r.take(1), 'assignment scope')
                    self.expression(r)
                elif op in self.native:
                    self.arguments(op, r)
                elif op == 0x17:
                    r.word()
                if r.left():
                    raise ValueError('unmapped payload')
            except ValueError as error:
                self.errors.add(f'{name}:{error}')


def condition(data, names):
    if len(data) not in (20, 24, 28):
        return {'error': 'unsupported CTDA length', 'length': len(data)}
    flags = data[0]
    function = struct.unpack_from('<H', data, 8)[0]
    result = {'function': function, 'name': names.get(function, f'function_{function}'),
              'flags': flags, 'a': form(data[12:16]), 'b': form(data[16:20]),
              'run_on': form(data[20:24]), 'reference': form(data[24:28])}
    if flags & 4:
        result['comparison_global'] = form(data[4:8])
    else:
        num = struct.unpack_from('<f', data, 4)[0]
        result['comparison'] = num if math.isfinite(num) else str(num)
    return result


def split_record(record):
    """Keep empty SCHR slots in position. Conditions belong to items, not bytecode ordinal."""
    kind = record['type']
    entries, gates = [], defaultdict(list)
    slot = 'script' if kind == 'SCPT' else 'begin' if kind == 'INFO' else 'unscoped'
    scope = {'INFO': 'info', 'PACK': 'package', 'QUST': 'quest', 'PERK': 'prerequisite'}.get(kind, 'record')
    stage = None
    item = -1
    effect = -1
    current = None
    for k, v in record['subs']:
        if k == 'CTDA':
            gates[scope].append(v)
        elif k == 'NEXT' and kind == 'INFO':
            slot = 'end'
        elif k == 'INDX' and kind == 'QUST':
            stage = int.from_bytes(v, 'little')
            item = -1
            scope = 'stage'
        elif k == 'QSDT' and kind == 'QUST':
            item += 1
            slot = f'{stage}/{item}'
            scope = 'item/' + slot
        elif k == 'QOBJ' and kind == 'QUST':
            scope = 'objective'
        elif k == 'ITXT' and kind == 'TERM':
            item += 1
            slot = str(item)
            scope = 'entry/' + slot
        elif k in ('POBA', 'POEA', 'POCA') and kind == 'PACK':
            slot = str(('POBA', 'POEA', 'POCA').index(k))
        elif k == 'PRKE' and kind == 'PERK':
            effect += 1
            slot = str(effect)
            scope = 'effect/' + slot
        elif k == 'PRKC' and kind == 'PERK':
            scope = f'effect/{effect}/group/{int.from_bytes(v, "little")}'
        elif k == 'PRKF' and kind == 'PERK':
            scope = 'prerequisite'
        elif k in ('SCHR', 'SCHD'):
            current = {'slot': slot, 'refs': [], 'locals': {}}
            entries.append(current)
        elif k in SCRIPT_FIELDS and current is not None:
            if k in ('SCRO', 'SCRV'):
                current['refs'].append((k, v))
            elif k == 'SLSD' and len(v) >= 4:
                current['last_local'] = form(v)
            elif k == 'SCVR' and 'last_local' in current:
                current['locals'][current['last_local']] = v.rstrip(b'\0').decode('cp1252', 'replace')
            elif k == 'SCDA':
                if 'data' in current:
                    raise ValueError(f"duplicate SCDA in {record['id']:08X}/{slot}")
                current['data'] = v
            elif k == 'SCTX':
                current['source'] = v
        elif k == 'SCDA' and v:
            raise ValueError(f"orphan SCDA in {record['id']:08X}/{slot}")
    return [e for e in entries if e.get('data')], gates


def read_probe(lines):
    evidence = defaultdict(dict)
    for line in lines:
        path, field, value = line.rstrip('\n').split('\t', 2)
        root, _, block = path.partition('@')
        target = evidence[root]
        if block:
            target = target.setdefault('blocks', {}).setdefault(block, {})
        if field in ('owner', 'event', 'compile', 'preview', 'filter'):
            if field in target:
                raise ValueError(f'duplicate probe row {path}/{field}')
            target[field] = int(value) if field == 'owner' else value
        elif field == 'condition':
            scope, index, function, result = value.split(':', 3)
            target.setdefault('conditions', {})[(scope, int(index))] = (int(function), result)
        else:
            target.setdefault(field, set()).add(value)
    return evidence


def diagnostic(message, aliases):
    # Keep native diagnoses queryable without distributing snippets of original scripts.
    message = message.split(' [', 1)[0]
    unknown = re.search(r'unsupported command: ([\w.]+)', message, re.I)
    if unknown:
        token = unknown[1].rsplit('.', 1)[-1].lower()
        if token in aliases:
            return 'unsupported command form/' + aliases[token]
    operand = re.search(r'unsupported expression operand: ([\w.]+)', message, re.I)
    if operand:
        name = operand[1].rsplit('.', 1)[-1].lower()
        if name in aliases:
            return 'unsupported expression operand/' + aliases[name]
        return 'unresolved expression operand'
    for prefix in ('unsupported command arguments:', 'unsupported executed command:'):
        if prefix in message:
            suffix = message.split(prefix, 1)[1].strip()
            if suffix:
                op = suffix.split()[0].rsplit('.', 1)[-1].lower()
                reason = suffix.partition(' (')[2].rstrip(')')
                return (prefix[:-1] + '/' + aliases.get(op, 'other') +
                        ('/' + diagnostic(reason, aliases) if reason else ''))
    if ' (' in message and message.endswith(')'):
        command, _, reason = message.partition(' (')
        head = command.split()[0].rsplit('.', 1)[-1].lower()
        if head in aliases:
            return aliases[head] + '/' + diagnostic(reason[:-1], aliases)
    message = re.sub(r'^(?:QUST \d+ stage \d+: |item \d+: |line \d+: )+', '', message)
    # Parameter values, EDIDs and source text are unnecessary for grouping.
    message = message.split(':')[0].split(' (')[0].split(" '")[0]
    return re.sub(r'\b\d+\b', '#', message)[:120] or 'unspecified'


def prepare_evidence(target, aliases, observations):
    for field in ('compiler_error', 'line_diagnostic', 'deferred_error', 'preview_error'):
        if field not in target:
            continue
        clean = sorted({diagnostic(s, aliases) for s in target[field]})
        target[field] = clean
        for reason in clean:
            # A rejected stage preview may originate in another item or nested
            # script. Keep it as context, never charge it as a direct missing command.
            if field != 'preview_error':
                observations.add(reason)
    if 'expression' in target:
        evaluations = set()
        for raw in target.pop('expression'):
            status, text = raw.split('\x1f', 1)
            # Keep identifiers of native calls, never the original script expression.
            tokens = re.findall(r'[a-zA-Z_][a-zA-Z_0-9]*', text.lower())
            calls = tuple(sorted({aliases[t] for t in tokens if t in aliases}))
            reason = 'bound' if status == 'bound' else diagnostic(status, aliases)
            if reason.startswith('unsupported expression operand/'):
                observations.add(reason)
            evaluations.add((calls, reason))
        target['expressions'] = [{'native_names': calls, 'binding': reason} for calls, reason in sorted(evaluations)]
    if 'bound_form' in target:
        target['bound_forms'] = sorted(map(int, target.pop('bound_form')))
    for block in target.get('blocks', {}).values():
        prepare_evidence(block, aliases, observations)


def scoped_conditions(kind, slot, gates, parent):
    if kind == 'INFO':
        return [('info', gates['info']), ('parent_quest', parent)]
    if kind == 'PACK':
        return [('package', gates['package'])]
    if kind == 'TERM':
        return [('entry', gates['entry/' + slot])]
    if kind == 'QUST':
        return [('item', gates['item/' + slot])]
    if kind == 'PERK':
        return [(s, g) for s, g in gates.items() if s == 'prerequisite' or s == 'effect/' + slot or s.startswith('effect/' + slot + '/')]
    return []


def fragment_rows(data, commands, probes):
    native = {op['opcode']: op for op in commands}
    condition_names = {op['condition_id']: op['name'] for op in commands if op['condition_id'] is not None}
    aliases = {n.lower(): op['name'] for op in commands for n in (op['name'], op['alias']) if n}
    known = set(native) | set(range(0x10, 0x20))
    forms = {}
    parents = {}
    for record in records(data):
        forms[record['id']] = record['type']
        if record['type'] == 'QUST':
            _, gates = split_record(record)
            parents[record['id']] = gates['quest']
    seen = set()
    for record in records(data):
        if not any(values(record, 'SCDA')):
            continue
        kind = record['type']
        entries, gates = split_record(record)
        for index, entry in enumerate(entries):
            slot = entry['slot']
            key = f"{kind}:{record['id']:08X}:{slot}"
            if key in seen:
                raise ValueError(f'duplicate fragment identity {key}')
            seen.add(key)
            deps = []
            for tag, raw in entry['refs']:
                value = form(raw)
                ref = {'kind': tag, 'value': value, 'uses': set()}
                if len(raw) != 4:
                    ref['resolution'] = 'invalid_width'
                elif tag == 'SCRO':
                    ref['resolution'] = forms.get(value, 'unresolved_form')
                else:
                    ref['resolution'] = entry['locals'].get(value, 'owner_local_at_runtime')
                deps.append(ref)
            scan = Dependencies(native, deps)
            try:
                scan.scan(entry['data'], known)
            except ValueError as error:
                scan.errors.add('envelope:' + str(error))
            for ref in deps:
                ref['uses'] = sorted(ref['uses'])
            observed = probes.pop(key, {})
            raw_gates = observed.pop('conditions', {})
            needs = set()
            prepare_evidence(observed, aliases, needs)
            parent = parents.get(form(first(record, 'QSTI')), [])
            checks = {}
            for scope, group in scoped_conditions(kind, slot, gates, parent):
                result = []
                for pos, raw in enumerate(group):
                    c = condition(raw, condition_names)
                    run = raw_gates.pop((scope, pos), None)
                    if run:
                        if c.get('function') != run[0]:
                            raise ValueError(f'condition probe identity mismatch at {key}/{scope}/{pos}')
                        c['probe'] = run[1]
                        if run[1].startswith('unsupported function '):
                            needs.add('unsupported condition/' + c['name'])
                        elif run[1] == 'unsupported run-on context':
                            needs.add('unsupported condition run-on/' + str(c['run_on']))
                    result.append(c)
                if result:
                    checks[scope] = result
            if raw_gates:
                raise ValueError(f'unmatched condition probe at {key}')
            source_events = []
            for line_no, line in enumerate(entry.get('source', b'').splitlines(), 1):
                match = re.match(rb'\s*begin\s+([\w]+)(?:\s+([\w.]+))?', line.split(b';', 1)[0], re.I)
                if match:
                    event = {'name': match[1].decode('ascii').lower(), 'line': line_no}
                    if match[2]:
                        event['filter'] = match[2].decode('ascii').lower()
                    source_events.append(event)
            yield {'id': key, 'index': index, 'bytecode_bytes': len(entry['data']),
                   'commands': sorted(scan.commands), 'expression_calls': sorted(scan.queries),
                   'operators': sorted(scan.operators), 'references': deps,
                   'variable_accesses': sorted(scan.locals), 'declared_locals': entry['locals'],
                   'event_headers': scan.events, 'declared_events': source_events, 'conditions': checks,
                   'unmapped_operands': sorted(scan.errors), 'runtime': observed, 'observed_gaps': sorted(needs)}
    if probes:
        raise ValueError(f'{len(probes)} unmatched runtime fragments, first: {min(probes)}')


def summarize(rows):
    counts = {name: Counter() for name in ('record_types', 'compilation', 'preview', 'observed_gaps',
                                           'commands', 'expression_calls', 'events', 'conditions', 'unmapped_operands',
                                           'preview_rejections', 'reference_kinds', 'bound_expression_calls', 'handler_diagnostics',
                                           'condition_evaluated', 'operand_coverage')}
    for row in rows:
        root = row['runtime']
        units = list(root.get('blocks', {}).values()) or [root]
        for field, dest in [('compile', 'compilation'), ('preview', 'preview')]:
            states = {unit.get(field, 'unprobed') for unit in units}
            counts[dest][next(iter(states)) if len(states) == 1 else 'mixed_event_blocks'] += 1
        counts['record_types'][row['id'].split(':')[0]] += 1
        for name in ('commands', 'expression_calls', 'observed_gaps', 'unmapped_operands'):
            counts[name].update(set(row[name]))
        counts['handler_diagnostics'].update({need for need in row['observed_gaps'] if need.startswith(
            ('unsupported command form/', 'unsupported expression operand/', 'unsupported condition/'))})
        counts['bound_expression_calls'].update({n for unit in units for e in unit.get('expressions', [])
            if e['binding'] == 'bound' for n in e['native_names'] if n in row['expression_calls']})
        counts['condition_evaluated'].update({c['name'] for group in row['conditions'].values() for c in group
                                              if c.get('probe') in ('true', 'false')})
        counts['operand_coverage']['has_unmapped_operands' if row['unmapped_operands'] else 'all_mapped'] += 1
        counts['events'].update({e['name'] for e in row['declared_events']})
        counts['conditions'].update({c['name'] for group in row['conditions'].values() for c in group if 'name' in c})
        counts['reference_kinds'].update({r['kind'] for r in row['references']})
        counts['preview_rejections'].update({r for unit in units for r in unit.get('preview_error', [])})
    return {name: dict(sorted(c.items(), key=lambda item: (-item[1], item[0]))) for name, c in counts.items()}


def event_dispatch_sites(root, event_names):
    """Locate literal producer calls in the production tree; indirect/dynamic calls can be absent."""
    emit = re.compile(r'\b(?:DispatchEquipmentEvent|DispatchReferenceEvent|RunSourceEventDirect|RunSourceEvent|Event|run)\(')
    literal = re.compile(r'"([a-zA-Z]+)"')
    found = {event: [] for event in event_names}
    for path in sorted(root.rglob('*')):
        if path.suffix not in ('.cpp', '.inc', '.h'):
            continue
        for number, line in enumerate(path.read_text(errors='replace').splitlines(), 1):
            code = line.split('//', 1)[0]
            for call in emit.finditer(code):
                # Bound literals to this call, including nested () or a ternary event argument.
                depth = 1
                end = call.end()
                while end < len(code) and depth:
                    depth += int(code[end] == '(') - int(code[end] == ')')
                    end += 1
                for name in literal.findall(code[call.end():end]):
                    event = name.lower()
                    if event in found:
                        site = f'{path.relative_to(root)}:{number}'
                        if site not in found[event]:
                            found[event].append(site)
    return found
