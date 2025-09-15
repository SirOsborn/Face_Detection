from flask import Flask, request, jsonify, send_from_directory
import torch
from facenet_pytorch import InceptionResnetV1
from torchvision import transforms
from PIL import Image
import numpy as np
import os
import pickle
import csv
from datetime import datetime

IMG_SIZE = 160
EMBEDDINGS_PATH = 'registered_students/embeddings.pkl'

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

@app.route('/register', methods=['POST'])
def register_student():
    if 'image' not in request.files or 'name' not in request.form or 'student_id' not in request.form:
        return jsonify({'error': 'Missing image or student info'}), 400
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
            writer.writerow(['student_id', 'name', 'image_path', 'timestamp'])
        writer.writerow([student_id, name, img_path, timestamp])
    # Extract face embedding
    img = Image.open(img_path).convert('RGB')
    img_tensor = preprocess_image(img)
    with torch.no_grad():
        embedding = embedding_model(img_tensor).squeeze().numpy()
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
        attendance_dir = 'attendance_records'
        os.makedirs(attendance_dir, exist_ok=True)
        attendance_csv = os.path.join(attendance_dir, 'attendance.csv')
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        file_exists = os.path.isfile(attendance_csv)
        with open(attendance_csv, 'a', newline='', encoding='utf-8') as csvfile:
            writer = csv.writer(csvfile)
            if not file_exists:
                writer.writerow(['student_id', 'name', 'timestamp'])
            writer.writerow([best_match[0], best_match[1]['name'], timestamp])
        return jsonify({
            'status': 'attendance_marked',
            'recognized': True,
            'student_id': best_match[0],
            'name': best_match[1]['name'],
            'score': float(best_score),
            'timestamp': timestamp,
            'message': f"Attendance marked for {best_match[1]['name']} ({best_match[0]})"
        })
    else:
        return jsonify({
            'status': 'attendance_failed',
            'recognized': False,
            'score': float(best_score),
            'message': 'Face not recognized. Attendance not marked.'
        })

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)

from flask import Flask, request, jsonify, send_from_directory
import torch
from facenet_pytorch import InceptionResnetV1
from torchvision import transforms
from PIL import Image
import numpy as np
import os
import pickle
import csv
from datetime import datetime


IMG_SIZE = 160
EMBEDDINGS_PATH = 'registered_students/embeddings.pkl'


app = Flask(__name__)
# Load facenet model for embeddings
embedding_model = InceptionResnetV1(pretrained='vggface2').eval()

# Serve registration page
@app.route('/')
def serve_registration():
    return send_from_directory('.', 'register.html')


# Registration endpoint: save image, student info, and embedding
@app.route('/register', methods=['POST'])
def register_student():
    if 'image' not in request.files or 'name' not in request.form or 'student_id' not in request.form:
        return jsonify({'error': 'Missing image or student info'}), 400
    file = request.files['image']
    name = request.form['name']
    student_id = request.form['student_id']
    # Save image to folder
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
            writer.writerow(['student_id', 'name', 'image_path', 'timestamp'])
        writer.writerow([student_id, name, img_path, timestamp])

    # Extract face embedding
    img = Image.open(img_path).convert('RGB')
    preprocess = transforms.Compose([
        transforms.Resize((IMG_SIZE, IMG_SIZE)),
        transforms.ToTensor(),
        transforms.Normalize([0.5, 0.5, 0.5], [0.5, 0.5, 0.5])
    ])
    img_tensor = preprocess(img).unsqueeze(0)
    with torch.no_grad():
        embedding = embedding_model(img_tensor).squeeze().numpy()

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




# Recognition endpoint: identify student from image
@app.route('/detect', methods=['POST'])
def detect_student():
    if 'image' not in request.files:
        return jsonify({'error': 'Missing image'}), 400
    file = request.files['image']
    img = Image.open(file.stream).convert('RGB')
    preprocess = transforms.Compose([
        transforms.Resize((IMG_SIZE, IMG_SIZE)),
        transforms.ToTensor(),
        transforms.Normalize([0.5, 0.5, 0.5], [0.5, 0.5, 0.5])
    ])
    img_tensor = preprocess(img).unsqueeze(0)
    with torch.no_grad():
        query_embedding = embedding_model(img_tensor).squeeze().numpy()

    # Load stored embeddings
    if not os.path.exists(EMBEDDINGS_PATH):
        return jsonify({'error': 'No registered students'}), 404
    with open(EMBEDDINGS_PATH, 'rb') as f:
        embeddings_db = pickle.load(f)

    # Compare query embedding to all stored embeddings
    def cosine_similarity(a, b):
        return np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b))

    best_match = None
    best_score = -1
    for student_id, info in embeddings_db.items():
        score = cosine_similarity(query_embedding, info['embedding'])
        if score > best_score:
            best_score = score
            best_match = (student_id, info)

    # Threshold for recognition (tune as needed)
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

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
