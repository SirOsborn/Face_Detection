# train.py - Face Detection Model Training Script

import os
import numpy as np
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import LabelEncoder
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
from torchvision import models, transforms
from PIL import Image

DATA_DIR = 'images/lfw-deepfunneled'
IMG_SIZE = 224
BATCH_SIZE = 64
EPOCHS = 10
DEVICE = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
print(f"Using device: {DEVICE}")

class FaceDataset(Dataset):
    def __init__(self, image_paths, labels, transform=None):
        self.image_paths = image_paths
        self.labels = labels.astype(np.int64)
        self.transform = transform
    def __len__(self):
        return len(self.image_paths)
    def __getitem__(self, idx):
        img = Image.open(self.image_paths[idx]).convert('RGB')
        if self.transform:
            img = self.transform(img)
        return img, torch.tensor(self.labels[idx])

# Collect image paths and labels

# Filter out classes with fewer than min_images_per_class
min_images_per_class = 20
image_paths, labels = [], []
class_counts = {}
for label in os.listdir(DATA_DIR):
    label_dir = os.path.join(DATA_DIR, label)
    if not os.path.isdir(label_dir):
        continue
    img_names = os.listdir(label_dir)
    if len(img_names) < min_images_per_class:
        continue
    for img_name in img_names:
        img_path = os.path.join(label_dir, img_name)
        image_paths.append(img_path)
        labels.append(label)
    class_counts[label] = len(img_names)
image_paths = np.array(image_paths)
labels = np.array(labels)

# Encode string labels to integers


le = LabelEncoder()
labels_encoded = le.fit_transform(labels)

# Save label encoder for inference
import pickle
with open('labels.pkl', 'wb') as f:
    pickle.dump(le, f)

# Split dataset
X_trainval, X_test, y_trainval, y_test = train_test_split(image_paths, labels_encoded, test_size=0.2, random_state=42)
# Further split train into train/val
X_train, X_val, y_train, y_val = train_test_split(X_trainval, y_trainval, test_size=0.15, random_state=42)

# Data augmentation and normalization
train_transform = transforms.Compose([
    transforms.Resize((IMG_SIZE, IMG_SIZE)),
    transforms.RandomHorizontalFlip(),
    transforms.RandomRotation(20),
    transforms.ColorJitter(brightness=0.2, contrast=0.2, saturation=0.2, hue=0.1),
    transforms.RandomResizedCrop(IMG_SIZE, scale=(0.8, 1.0)),
    transforms.ToTensor(),
    transforms.Normalize([0.485, 0.456, 0.406], [0.229, 0.224, 0.225])
])
test_transform = transforms.Compose([
    transforms.Resize((IMG_SIZE, IMG_SIZE)),
    transforms.ToTensor(),
    transforms.Normalize([0.485, 0.456, 0.406], [0.229, 0.224, 0.225])
])

train_dataset = FaceDataset(X_train, y_train, transform=train_transform)
val_dataset = FaceDataset(X_val, y_val, transform=test_transform)
test_dataset = FaceDataset(X_test, y_test, transform=test_transform)
train_loader = DataLoader(train_dataset, batch_size=BATCH_SIZE, shuffle=True)
val_loader = DataLoader(val_dataset, batch_size=BATCH_SIZE, shuffle=False)
test_loader = DataLoader(test_dataset, batch_size=BATCH_SIZE, shuffle=False)

# Load pre-trained ResNet18 and fine-tune

# Use ResNet50 for deeper model
num_classes = len(le.classes_)
model = models.resnet50(weights=models.ResNet50_Weights.DEFAULT)
model.fc = nn.Linear(model.fc.in_features, num_classes)
# Unfreeze last 2 blocks for fine-tuning
for name, param in model.named_parameters():
    if "layer4" in name or "layer3" in name or "fc" in name:
        param.requires_grad = True
    else:
        param.requires_grad = False
model = model.to(DEVICE)

# Compute class weights for imbalanced data
from collections import Counter
class_sample_count = np.array([class_counts[cls] for cls in le.classes_])
class_weights = 1. / class_sample_count
weights = torch.tensor(class_weights, dtype=torch.float).to(DEVICE)
criterion = nn.CrossEntropyLoss(weight=weights)
optimizer = optim.Adam(filter(lambda p: p.requires_grad, model.parameters()), lr=0.0002)
scheduler = optim.lr_scheduler.ReduceLROnPlateau(optimizer, mode='min', factor=0.5, patience=3)


# Training loop with validation and early stopping
EPOCHS = 30
best_val_loss = float('inf')
patience = 6
wait = 0
for epoch in range(EPOCHS):
    model.train()
    running_loss = 0.0
    for imgs, lbls in train_loader:
        imgs, lbls = imgs.to(DEVICE), lbls.to(DEVICE)
        optimizer.zero_grad()
        outputs = model(imgs)
        loss = criterion(outputs, lbls)
        loss.backward()
        optimizer.step()
        running_loss += loss.item() * imgs.size(0)
    epoch_loss = running_loss / len(train_loader.dataset)

    # Validation
    model.eval()
    val_loss = 0.0
    correct = 0
    total = 0
    with torch.no_grad():
        for imgs, lbls in val_loader:
            imgs, lbls = imgs.to(DEVICE), lbls.to(DEVICE)
            outputs = model(imgs)
            loss = criterion(outputs, lbls)
            val_loss += loss.item() * imgs.size(0)
            _, predicted = torch.max(outputs.data, 1)
            total += lbls.size(0)
            correct += (predicted == lbls).sum().item()
    val_loss /= len(val_loader.dataset)
    val_acc = 100 * correct / total
    print(f'Epoch {epoch+1}/{EPOCHS}, Train Loss: {epoch_loss:.4f}, Val Loss: {val_loss:.4f}, Val Acc: {val_acc:.2f}%')
    scheduler.step(val_loss)
    # Early stopping
    if val_loss < best_val_loss:
        best_val_loss = val_loss
        wait = 0
        torch.save(model.state_dict(), 'face_detection_resnet50.pt')
    else:
        wait += 1
        if wait >= patience:
            print("Early stopping triggered.")
            break

# Evaluate model accuracy on test set
model.load_state_dict(torch.load('face_detection_resnet50.pt'))
model.eval()
correct = 0
total = 0
with torch.no_grad():
    for imgs, lbls in test_loader:
        imgs, lbls = imgs.to(DEVICE), lbls.to(DEVICE)
        outputs = model(imgs)
        _, predicted = torch.max(outputs.data, 1)
        total += lbls.size(0)
        correct += (predicted == lbls).sum().item()
accuracy = 100 * correct / total
print(f'Model test accuracy: {accuracy:.2f}%')
