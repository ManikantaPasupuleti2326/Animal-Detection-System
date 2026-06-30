import os
import time
import cv2
from firebase_admin import storage
from ultralytics import YOLO
import firebase
from twilio.rest import Client
from collections import Counter

os.makedirs('downloads', exist_ok=True)

FIREBASE_CAM_PATH_PREFIX = "CAM"
LOCAL_DOWNLOAD_DIR = "downloads"
TWILIO_ACCOUNT_SID = "Your Account SID"
TWILIO_AUTH_TOKEN = "Your Auth Token"
TO_NUMBER = "To Number"
FROM_NUMBER = "give receiver Number"

class FirebaseHandler:
    def __init__(self):
        firebase.run()
        self.bucket = storage.bucket()

    def get_camera_folders(self):
        blobs = self.bucket.list_blobs()
        cam_folders = set()
        for blob in blobs:
            parts = blob.name.split("/")
            if len(parts) > 1 and parts[0].startswith(FIREBASE_CAM_PATH_PREFIX):
                cam_folders.add(parts[0])
        return list(cam_folders)

    def download_image(self, cam_folder, filename="photo.jp"):
        blob_path = f"{cam_folder}/{filename}"
        blob = self.bucket.blob(blob_path)
        if blob.exists():
            local_file = os.path.join(LOCAL_DOWNLOAD_DIR, f"{cam_folder}_photo.jpg")
            blob.download_to_filename(local_file)
            print(f"[{cam_folder}] Image downloaded: {local_file}")
            return local_file, blob
        else:
            print(f"[{cam_folder}] No image found.")
            return None, None

    def delete_blob(self, blob):
        blob.delete()
        print("Image deleted from Firebase.\n")

class YOLODetector:
    def __init__(self, model_path="models/best.pt"):
        self.model = YOLO(model_path)

    def detect(self, image_path):
        results = self.model(image_path)
        detected_objects = []
        if results:
            for r in results:
                detected_objects.extend([r.names[int(cls)] for cls in r.boxes.cls])
        return detected_objects, results[0].plot() if results else None

class AlertSystem:
    def __init__(self):
        self.client = Client(TWILIO_ACCOUNT_SID, TWILIO_AUTH_TOKEN)

    def make_call(self, detected_animals, cam_name):
        """
        Makes a Twilio voice call announcing detected animals with counts.
        """
        counts = Counter(detected_animals)

        formatted = []
        for animal, count in counts.items():
            name = animal if count == 1 else f"{animal}s"
            formatted.append(f"{count} {name}")
        animals_text = ", ".join(formatted)

        message_text = f"Alert! Animals detected in {cam_name}: {animals_text}"

        call = self.client.calls.create(
            to=TO_NUMBER,
            from_=FROM_NUMBER,
            twiml=f'<Response><Say voice="man">{message_text}</Say></Response>'
        )
        print(f"Voice call triggered, SID: {call.sid}")

class AnimalDetectionApp:
    def __init__(self):
        self.firebase = FirebaseHandler()
        self.detector = YOLODetector()
        self.alert = AlertSystem()

    def run(self):
        while True:
            camera_list = self.firebase.get_camera_folders()

            all_detections = {}

            for cam in camera_list:
                local_file, blob = self.firebase.download_image(cam)
                if local_file:
                    detected_animals, annotated_image = self.detector.detect(local_file)
                    if detected_animals:
                        print(f"[{cam}] Detected animals: {detected_animals}")

                        alert_image_path = os.path.join(LOCAL_DOWNLOAD_DIR, f"{cam}_alert.jpg")
                        cv2.imwrite(alert_image_path, annotated_image)

                        all_detections[cam] = detected_animals
                    else:
                        print(f"[{cam}] No objects detected.")
                    self.firebase.delete_blob(blob)

            if all_detections:
                try:
                    full_message = []
                    for cam, animals in all_detections.items():
                        counts = Counter(animals)
                        formatted = []
                        for animal, count in counts.items():
                            name = animal if count == 1 else f"{animal}s"
                            formatted.append(f"{count} {name}")
                        full_message.append(f"{cam}: {', '.join(formatted)}")

                    final_message = " | ".join(full_message)
                    self.alert.make_call([final_message], "Multiple Cameras")
                except Exception as e:
                    print("Only Verified members")

            #time.sleep(2)

if __name__ == "__main__":
    app = AnimalDetectionApp()
    app.run()
