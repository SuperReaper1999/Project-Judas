"""Verify the separately authorized WorldOrigin mechanical patch, without a build."""
from pathlib import Path
import hashlib
import importlib.util
import json
import sys
sys.dont_write_bytecode = True

HERE = Path(__file__).resolve().parents[1]
ROOT = next(parent for parent in HERE.parents if (parent / 'engine-version.json').is_file())
spec = importlib.util.spec_from_file_location('refactor_tokens', HERE / 'tools/verify_refactor.py')
lexer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lexer)
before = (HERE / 'WorldOrigin.before-format.cpp').read_text()
after = (ROOT / 'src/WorldOrigin.cpp').read_text()
assert lexer.tokens(before) == lexer.tokens(after), 'WorldOrigin changed beyond formatting'
changed = after.replace('return false;', 'return true;', 1)
assert changed != after, 'negative mutation was not applied'
assert lexer.tokens(before) != lexer.tokens(changed), 'changed failure behavior was not detected'
print(json.dumps({'tokens_identical': True, 'tokens': len(lexer.tokens(after)),
                  'token_sha256': lexer.digest(lexer.tokens(after)),
                  'changed_failure_control': 'detected'}, indent=2))
