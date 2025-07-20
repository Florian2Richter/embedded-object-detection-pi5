import argparse
import cv2
import numpy as np
import onnxruntime as ort


def draw_detections(frame, detections, score_thresh=0.4):
    """Draw bounding boxes on the frame.
    The model output is expected as Nx6: x1, y1, x2, y2, score, class."""
    if detections is None:
        return
    for det in detections:
        if len(det) < 6 or det[4] < score_thresh:
            continue
        x1, y1, x2, y2 = map(int, det[:4])
        cls = int(det[5]) if len(det) > 5 else 0
        cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 255, 0), 2)
        cv2.putText(frame, str(cls), (x1, y1 - 5), cv2.FONT_HERSHEY_SIMPLEX,
                    0.5, (0, 255, 0), 1)


def main():
    parser = argparse.ArgumentParser(description="Run object detection on a video file")
    parser.add_argument("source", help="Path to video file or camera index")
    parser.add_argument("--model", default="model/model.onnx", help="Path to ONNX model")
    parser.add_argument("--score-thresh", type=float, default=0.4, help="Score threshold")
    args = parser.parse_args()

    try:
        cam_index = int(args.source)
        cap = cv2.VideoCapture(cam_index)
    except ValueError:
        cap = cv2.VideoCapture(args.source)

    if not cap.isOpened():
        print(f"Failed to open {args.source}")
        return

    session = ort.InferenceSession(args.model)
    input_name = session.get_inputs()[0].name

    while True:
        ret, frame = cap.read()
        if not ret:
            break
        img = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        img = cv2.resize(img, (640, 640))
        img = img.transpose(2, 0, 1).astype(np.float32)
        img = np.expand_dims(img, 0)

        outputs = session.run(None, {input_name: img})
        detections = outputs[0] if outputs else None
        draw_detections(frame, detections, args.score_thresh)

        cv2.imshow("detections", frame)
        if cv2.waitKey(1) & 0xFF == 27:
            break

    cap.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
