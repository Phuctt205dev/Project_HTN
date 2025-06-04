# Nhập các thư viện cần thiết:
import paho.mqtt.client as mqtt  # Thư viện dùng để giao tiếp MQTT (gửi dữ liệu đến ESP32)
import time                      # Dùng để chờ tạm thời nếu có lỗi
import cv2                       # OpenCV để xử lý hình ảnh từ camera

# Địa chỉ MQTT Broker và thông tin cấu hình
broker = "172.20.10.2"           # Địa chỉ IP của máy chủ MQTT (chạy trên ESP32 hoặc laptop)
port = 1883                      # Cổng mặc định của MQTT
topic = "vehicles/road1"         # Chủ đề MQTT để gửi số lượng xe ở Đường 1
stream_url = "http://172.20.10.5:81/stream"  # Địa chỉ video stream từ ESP32-CAM

# Tạo client MQTT và kết nối
client = mqtt.Client()
client.connect(broker, port)

# Lưu số lượng xe cũ để tránh gửi dữ liệu trùng lặp
previous_vehicles = -1

# Hàm xử lý từng khung hình (frame) từ camera
def process_frame(frame):
    global previous_vehicles  # Dùng biến toàn cục để so sánh xe cũ/mới

    # Bước 1: Chuyển ảnh sang đen trắng
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

    # Bước 2: Làm mờ ảnh để loại bỏ nhiễu
    blurred = cv2.GaussianBlur(gray, (5, 5), 0)

    # Bước 3: Dò biên cạnh trong ảnh (nhận diện vật thể)
    edges = cv2.Canny(blurred, 50, 150)

    # Bước 4: Tìm các đường viền (contours) của vật thể trong ảnh
    contours, _ = cv2.findContours(edges, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

    vehicles = 0  # Đếm số xe

    # Duyệt qua từng contour để kiểm tra xem có phải xe không
    for contour in contours:
        # Làm mượt các đường viền thành đa giác
        approx = cv2.approxPolyDP(contour, 0.04 * cv2.arcLength(contour, True), True)

        # Nếu đa giác có 4 cạnh và diện tích lớn hơn 100 pixel thì coi là một xe
        if len(approx) == 4 and cv2.contourArea(contour) > 100:
            vehicles += 1  # Tăng biến đếm xe
            # Vẽ viền xanh quanh xe trên ảnh
            cv2.drawContours(frame, [approx], -1, (0, 255, 0), 2)

    # Nếu số xe thay đổi thì gửi dữ liệu mới lên MQTT
    if vehicles != previous_vehicles:
        client.publish(topic, vehicles)
        previous_vehicles = vehicles

    # Ghi số lượng xe lên góc trên ảnh
    cv2.putText(frame, f"Road-1: {vehicles}", (10, 30),
                cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)

    return frame  # Trả lại khung hình đã xử lý để hiển thị

# Hàm chính chạy liên tục
def main():
    print("Connecting to camera...")

    # Kết nối tới luồng video từ camera ESP32-CAM
    cap = cv2.VideoCapture(stream_url)

    # Nếu không kết nối được thì thoát
    if not cap.isOpened():
        print("Unable to connect to camera stream.")
        return

    # Vòng lặp xử lý liên tục từng khung hình
    while True:
        ret, frame = cap.read()  # Đọc một khung hình
        if not ret:
            print("Frame read failed. Retrying...")
            time.sleep(1)
            continue

        # Xử lý khung hình và hiển thị lên cửa sổ
        cv2.imshow('Vehicles Detection', process_frame(frame))

        # Nhấn phím 'q' để thoát chương trình
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    # Sau khi thoát, đóng kết nối và giải phóng bộ nhớ
    cap.release()
    cv2.destroyAllWindows()
    client.disconnect()

# Chạy chương trình nếu được gọi trực tiếp
if __name__ == "__main__":
    main()
