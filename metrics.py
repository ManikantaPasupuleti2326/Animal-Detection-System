# =============================================================
# YOLOv8 Model Evaluation Script
# Generates metrics, ROC curves, and saves results as CSV
# =============================================================

from ultralytics import YOLO
import numpy as np
import pandas as pd
from sklearn.metrics import roc_curve, auc
from sklearn.preprocessing import label_binarize
import matplotlib.pyplot as plt
import os

# -------------------------------------------------------------
# 1️⃣ Load model and run validation
# -------------------------------------------------------------
model = YOLO(r"app/models/AnimalModelFinal.pt")

results = model.val(
    data=r"dataset\Animal_Detection\data.yaml",
    split="val",
    save_json=True,
    save_hybrid=True
)

save_dir = "runs/detect/val/"
os.makedirs(save_dir, exist_ok=True)

# -------------------------------------------------------------
# 2️⃣ Print and save core metrics
# -------------------------------------------------------------
print(f"mAP50:      {results.box.map50:.4f}")
print(f"mAP50-95:   {results.box.map:.4f}")
print(f"Precision:  {results.box.mp:.4f}")
print(f"Recall:     {results.box.mr:.4f}")
print(f"F1-score:   {results.box.mf1:.4f}")

# Save overall metrics to CSV
overall_metrics = {
    "mAP50": [results.box.map50],
    "mAP50-95": [results.box.map],
    "Precision": [results.box.mp],
    "Recall": [results.box.mr],
    "F1-score": [results.box.mf1],
}
pd.DataFrame(overall_metrics).to_csv(f"{save_dir}/results.csv", index=False)
print("✅ Overall results saved to results.csv")

# -------------------------------------------------------------
# 3️⃣ Generate built-in plots
# -------------------------------------------------------------
results.plot_confusion_matrix(save_dir=save_dir)
results.plot_pr_curve(save_dir=save_dir)
results.plot_f1_curve(save_dir=save_dir)

# -------------------------------------------------------------
# 4️⃣ Per-class metrics
# -------------------------------------------------------------
class_names = list(model.names.values())
data = []
for i, name in enumerate(class_names):
    data.append({
        "Class": name,
        "mAP50": results.box.maps[i] if len(results.box.maps) > i else None,
        "Precision": results.box.p[i] if len(results.box.p) > i else None,
        "Recall": results.box.r[i] if len(results.box.r) > i else None,
        "F1-score": results.box.f1[i] if len(results.box.f1) > i else None,
    })
pd.DataFrame(data).to_csv(f"{save_dir}/results_per_class.csv", index=False)
print("✅ Per-class results saved to results_per_class.csv")

# -------------------------------------------------------------
# 5️⃣ ROC curve computation
# -------------------------------------------------------------
y_true = []
y_scores = []
y_pred_classes = []

# Each result corresponds to one image
for r in results.results:
    if not hasattr(r, 'boxes') or r.boxes is None:
        continue
    y_scores.extend(r.boxes.conf.cpu().numpy())
    y_pred_classes.extend(r.boxes.cls.cpu().numpy())
    # ground truth
    if hasattr(r, "names"):
        y_true.extend([r.names.index(name) for name in r.names])
    else:
        y_true.extend(r.boxes.cls.cpu().numpy())

y_true = np.array(y_true)
y_scores = np.array(y_scores)

# -------------------------------------------------------------
# 6️⃣ Plot ROC curve (binary or multi-class)
# -------------------------------------------------------------
if len(np.unique(y_true)) == 2:
    fpr, tpr, _ = roc_curve(y_true, y_scores, pos_label=1)
    roc_auc = auc(fpr, tpr)

    plt.figure()
    plt.plot(fpr, tpr, color='blue', lw=2, label=f"AUC = {roc_auc:.2f}")
    plt.plot([0, 1], [0, 1], 'k--')
    plt.xlabel('False Positive Rate')
    plt.ylabel('True Positive Rate')
    plt.title('Overall ROC Curve')
    plt.legend(loc="lower right")
    plt.savefig(f"{save_dir}/roc_curve.png", dpi=300)
    plt.show()
else:
    classes = list(range(len(model.names)))
    y_true_bin = label_binarize(y_true, classes=classes)
    n_classes = y_true_bin.shape[1]

    plt.figure()
    for i in range(n_classes):
        if y_true_bin[:, i].sum() == 0:
            continue
        fpr, tpr, _ = roc_curve(y_true_bin[:, i], y_scores)
        roc_auc = auc(fpr, tpr)
        plt.plot(fpr, tpr, lw=2, label=f'{model.names[i]} (AUC={roc_auc:.2f})')

    plt.plot([0, 1], [0, 1], 'k--')
    plt.xlabel('False Positive Rate')
    plt.ylabel('True Positive Rate')
    plt.title('ROC Curves per Class')
    plt.legend()
    plt.savefig(f"{save_dir}/roc_per_class.png", dpi=300)
    plt.show()

print("✅ ROC curve(s) saved successfully!")

# -------------------------------------------------------------
# 7️⃣ Summary
# -------------------------------------------------------------
print("\n✅ Evaluation complete! Check the folder below:")
print(save_dir)
