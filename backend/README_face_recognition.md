# Face Recognition System Workflow

## Registration
- Student uses the registration web app to submit their name, student ID, and a face photo.
- The backend saves the image and info, then extracts a face embedding using a pretrained model (facenet-pytorch).
- The embedding is stored in a database (pickle file) with the student's info.
- No retraining is needed for new students—just add their embedding.

## Recognition
- When a face is scanned (via /detect endpoint), the backend extracts its embedding.
- The embedding is compared to all stored embeddings using cosine similarity.
- If a match is found (similarity above threshold), the student is identified.
- If no match, the response is 'unknown'.

## Adding New Students
- Register new students at any time; their embeddings are added instantly.
- The system will recognize them on future scans.

## Model
- Uses facenet-pytorch (InceptionResnetV1 pretrained on VGGFace2) for robust face embeddings.
- No need to retrain for each new student.

## Endpoints
- `/register`: Accepts image, name, and student ID. Stores info and embedding.
- `/detect`: Accepts image. Returns student info if recognized, or 'unknown'.

## How to Use
1. Start the backend server: `python app.py`
2. Open the registration page in your browser.
3. Register students.
4. Use `/detect` endpoint to identify faces.

## Notes
- Embeddings and info are stored in `backend/registered_students/embeddings.pkl` and `students.csv`.
- You can tune the recognition threshold in the code for stricter or looser matching.
- For best results, use clear, front-facing photos.
