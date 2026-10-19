def load_manifest(path):
    data = json.loads(path.read_text())
    if data.get('format') != 1 or not isinstance(data.get('tests'), list):
        raise ValueError('unsupported validation manifest')
    names = set()
    for entry in data['tests']:
        name = entry.get('name', '')
        if not re.fullmatch(r'[a-z0-9-]+', name) or name in names:
            raise ValueError('invalid or duplicate test name')
        names.add(name)
        if not re.fullmatch(r'judas_[a-z0-9_]+', entry.get('target', '')):
            raise ValueError('invalid native target')
        if not entry.get('groups') or not all(isinstance(g, str) for g in entry['groups']):
            raise ValueError('test groups required')
        if not all(isinstance(a, str) for a in entry.get('args', [])):
            raise ValueError('test arguments must be strings')
    return data
