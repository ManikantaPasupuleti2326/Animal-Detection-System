from ultralytics import YOLO

model = YOLO("yolov8s.pt")

# train model
model.train(
    data="dataset/Animal_Detection/data.yaml",  # path to your yaml
    epochs=50,        # increase if needed
    imgsz=640,
    batch=16,
    device=0          # use GPU
)