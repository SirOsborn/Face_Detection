
import requests

# Test registration
reg_url = 'http://127.0.0.1:5000/register'
detect_url = 'http://127.0.0.1:5000/detect'

# Use a sample image for testing
img_path = 'registered_students/test01_20250914_000756.jpg'
name = 'Osborn'
student_id = 'test01'

with open(img_path, 'rb') as img_file:
    files = {'image': img_file}
    data = {'name': name, 'student_id': student_id}
    reg_resp = requests.post(reg_url, files=files, data=data)
    print('Registration response:', reg_resp.json())

# Test recognition
with open(img_path, 'rb') as img_file:
    files = {'image': img_file}
    detect_resp = requests.post(detect_url, files=files)
    print('Recognition response:', detect_resp.json())
