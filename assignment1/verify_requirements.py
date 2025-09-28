import os
import subprocess
import numpy as np          # pip install numpy
from PIL import Image       # pip install pillow
from PIL import ImageFont
from PIL import ImageDraw
import json
import click                # pip install click

_temp_file_name = '__verification.png'

font = ImageFont.truetype("assets/fonts/roboto_mono.ttf", 64)

def overlay(im: Image.Image, text: str):
    draw = ImageDraw.Draw(im)
    draw.text((0, 0), text, (255,255,255), font=font)
    return im


def check_reference(state_json: str, test_exe: str, output_differences: bool):
    with open(state_json, 'rt', encoding='UTF-8') as f:
        state = json.load(f)
        print(f'{state_json} ', end='')
        args = [test_exe, '--state', state_json, '--output', _temp_file_name]
        completed = subprocess.run(args, capture_output=True)
        if completed.returncode != 0:
            print(f'Error running reference state "{state_json}", stderr dump below')
            print(completed.stderr)
        else:
            student_image = Image.open(_temp_file_name).convert('RGB')
            reference_image = Image.open(state["reference_image"]).convert('RGB')
            assert student_image.size == reference_image.size
            
            sinp = np.array(student_image).astype(np.float32)
            refnp = np.array(reference_image).astype(np.float32)
            
            MSE = np.mean((sinp - refnp)**2)
            print(f'MSE={MSE:6.2f} ', end='')
            
            if MSE < 1.0:
                print(' PASS', end=' ' if output_differences else '\n')
            else:
                print(' FAIL', end=' ' if output_differences else '\n')
                
            if output_differences:
                difference_image = np.clip(128 + np.array(student_image).astype(np.int32) - np.array(reference_image).astype(np.int32), 0, 255).astype(np.uint8)
                
                w, h = student_image.width, student_image.height
                comp = np.zeros((h, 3*w, 3), dtype=np.uint8)
                comp[:, :w, :] = np.array(overlay(reference_image, 'Reference image'))
                comp[:, w:2*w, :] = np.array(overlay(student_image, 'Test image'))
                comp[:, 2*w:, :] = np.array(overlay(Image.fromarray(difference_image), 'Difference'))
                
                root, _ = os.path.splitext(state_json)
                diff_file = f'{root}_difference.png'
                Image.fromarray(comp).save(diff_file)
                
                print(f' => {diff_file}')
        
            os.remove(_temp_file_name)
            
            
@click.command()
@click.argument('test_exe', type=click.Path(exists=True, dir_okay=False, allow_dash=False))
@click.argument('states_to_check', type=str)
@click.option('--output_differences', type=bool, default=True)
def verify_requirements(test_exe: str, states_to_check: str, output_differences: bool):
    for s in [f'saved_states/reference_state_{int(s):02d}.json' for s in states_to_check.split(',')]:
        check_reference(s, test_exe, output_differences)


if __name__ == '__main__':
    verify_requirements()
