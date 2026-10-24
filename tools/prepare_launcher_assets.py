"""Package the selected logo for Tanmatsu's launcher metadata/assets folder."""
from pathlib import Path
import json,re
from PIL import Image
root=Path(__file__).resolve().parents[1]
version=int(re.search(r'MESH_APP_VERSION (\d+)',(root/'main/app_version.h').read_text())[1])
out=root/'assets/launcher';out.mkdir(exist_ok=True)
im=Image.open(root/'assets/branding/meshmatsu-icon-v2.png').convert('RGBA')
for size in (16,32,64):
 im.resize((size,size),Image.Resampling.LANCZOS).save(out/f'icon{size}.png',optimize=True)
metadata={'name':'MeshMatsu','description':'Meshtastic messaging for Tanmatsu','categories':['communication'],'version':str(version),'icon':{f'{s}x{s}':f'icon{s}.png' for s in (16,32,64)},'application':[{'targets':['tanmatsu','konsool'],'revision':version,'type':'appfs','executable':'application.bin','assets':[]}]}
(out/'metadata.json').write_text(json.dumps(metadata,indent=2)+'\n')
