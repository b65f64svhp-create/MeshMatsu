"""Load the app's extracted UI sections for legacy platform integration fixtures.

Pure controllers/menu models are tested directly. These adapters still need small
slices of the platform event loop; their locations may be main.c or a UI include.
Expand only extracted sections, leaving existing component includes intact.
"""
from pathlib import Path

EXTRACTED_SECTIONS = (
    'chat_actions_draw.inc', 'settings_menu_ui.inc',
    'settings_navigation.inc', 'settings_menu_draw.inc',
    'node_actions_draw.inc', 'settings_draw.inc',
)

def read_main_ui(root):
    folder = Path(root) / 'main'
    source = (folder / 'main.c').read_text()
    for name in EXTRACTED_SECTIONS:
        source = source.replace(f'#include "{name}"', (folder / name).read_text())
    return source
