from flask import Flask, request, jsonify, send_from_directory, Response
import torch
from facenet_pytorch import InceptionResnetV1
from torchvision import transforms
from PIL import Image
import numpy as np
import os
import pickle
import csv
from datetime import datetime
import requests

IMG_SIZE = 160
EMBEDDINGS_PATH = 'registered_students/embeddings.pkl'
ESP32_CAM_URL = 'http://192.168.10.20/stream'

app = Flask(__name__)
embedding_model = InceptionResnetV1(pretrained='vggface2').eval()

def preprocess_image(img):
    preprocess = transforms.Compose([
        transforms.Resize((IMG_SIZE, IMG_SIZE)),
        transforms.ToTensor(),
        transforms.Normalize([0.5, 0.5, 0.5], [0.5, 0.5, 0.5])
    ])
    return preprocess(img).unsqueeze(0)

def cosine_similarity(a, b):
    return np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b))

@app.route('/')
def serve_registration():
    return send_from_directory('.', 'register.html')

@app.route('/esp32_cam_feed')
def esp32_cam_feed():
    def generate():
        while True:
            response = requests.get(ESP32_CAM_URL, stream=True)
            if response.status_code == 200:
                yield (b'--frame\r\n'
                       b'Content-Type: image/jpeg\r\n\r\n' + response.content + b'\r\n')
            else:
                break
    return Response(generate(), mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/register', methods=['POST'])
def register_student():
    if 'image' not in request.files or 'name' not in request.form or 'student_id' not in request.form:
        return jsonify({'error': 'Missing required fields'}), 400
    file = request.files['image']
    name = request.form['name']
    student_id = request.form['student_id']
    reg_dir = 'registered_students'
    os.makedirs(reg_dir, exist_ok=True)
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    img_filename = f"{student_id}_{timestamp}.jpg"
    img_path = os.path.join(reg_dir, img_filename)
    file.save(img_path)
    # Save info to CSV
    csv_path = os.path.join(reg_dir, 'students.csv')
    file_exists = os.path.isfile(csv_path)
    with open(csv_path, 'a', newline='', encoding='utf-8') as csvfile:
        writer = csv.writer(csvfile)
        if not file_exists:
            writer.writerow(['Student ID', 'Name', 'Image Path', 'Timestamp'])
        writer.writerow([student_id, name, img_path, timestamp])
    # Extract face embedding
    img = Image.open(img_path).convert('RGB')
    img_tensor = preprocess_image(img)
    with torch.no_grad():
        embedding = embedding_model(img_tensor).numpy()
    # Save embedding with student info
    if os.path.exists(EMBEDDINGS_PATH):
        with open(EMBEDDINGS_PATH, 'rb') as f:
            embeddings_db = pickle.load(f)
    else:
        embeddings_db = {}
    embeddings_db[student_id] = {
        'name': name,
        'embedding': embedding,
        'image_path': img_path,
        'timestamp': timestamp
    }
    with open(EMBEDDINGS_PATH, 'wb') as f:
        pickle.dump(embeddings_db, f)
    return jsonify({'status': 'registered', 'student_id': student_id, 'name': name, 'image_path': img_path})

@app.route('/detect', methods=['POST'])
def detect_student():
    if 'image' not in request.files:
        return jsonify({'error': 'Missing image'}), 400
    file = request.files['image']
    img = Image.open(file.stream).convert('RGB')
    img_tensor = preprocess_image(img)
    with torch.no_grad():
        query_embedding = embedding_model(img_tensor).squeeze().numpy()
    if not os.path.exists(EMBEDDINGS_PATH):
        return jsonify({'error': 'No registered students'}), 404
    with open(EMBEDDINGS_PATH, 'rb') as f:
        embeddings_db = pickle.load(f)
    best_match = None
    best_score = -1
    for student_id, info in embeddings_db.items():
        score = cosine_similarity(query_embedding, info['embedding'])
        if score > best_score:
            best_score = score
            best_match = (student_id, info)
    threshold = 0.7
    if best_score >= threshold:
        return jsonify({
            'status': 'recognized',
            'student_id': best_match[0],
            'name': best_match[1]['name'],
            'score': float(best_score),
            'image_path': best_match[1]['image_path'],
            'timestamp': best_match[1]['timestamp']
        })
    else:
        return jsonify({'status': 'unknown', 'score': float(best_score)})

@app.route('/attendance', methods=['POST'])
def mark_attendance():
    if 'image' not in request.files:
        return jsonify({'error': 'Missing image file'}), 400
    file = request.files['image']
    img = Image.open(file.stream).convert('RGB')
    img_tensor = preprocess_image(img)
    with torch.no_grad():
        embedding = embedding_model(img_tensor).numpy()
    if not os.path.exists(EMBEDDINGS_PATH):
        return jsonify({'error': 'No registered students found'}), 404
    with open(EMBEDDINGS_PATH, 'rb') as f:
        embeddings_db = pickle.load(f)
    best_match = None
    best_score = -1
    for student_id, info in embeddings_db.items():
        score = cosine_similarity(embedding, info['embedding'])
        if score > best_score:
            best_match = {'student_id': student_id, 'name': info['name']}
            best_score = score
    threshold = 0.7
    if best_score >= threshold:
        attendance_dir = 'attendance_records'
        os.makedirs(attendance_dir, exist_ok=True)
        csv_path = os.path.join(attendance_dir, 'attendance.csv')
        file_exists = os.path.isfile(csv_path)
        with open(csv_path, 'a', newline='', encoding='utf-8') as csvfile:
            writer = csv.writer(csvfile)
            if not file_exists:
                writer.writerow(['Student ID', 'Name', 'Timestamp'])
            writer.writerow([best_match['student_id'], best_match['name'], datetime.now().strftime('%Y-%m-%d %H:%M:%S')])
        return jsonify({'status': 'attendance marked', 'student_id': best_match['student_id'], 'name': best_match['name']})
    else:
        return jsonify({'status': 'no match found'})

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
