# Nhập thư viện cần thiết
import paho.mqtt.client as mqtt  # Thư viện để gửi dữ liệu MQTT
import time                      # Thư viện thời gian (dùng để chờ đợi)
import cv2                       # Thư viện OpenCV để xử lý hình ảnh

# Cấu hình kết nối MQTT
broker = "172.20.10.2"           # Địa chỉ IP của máy chủ MQTT
port = 1883                      # Cổng mặc định của MQTT
topic = "vehicles/road2"         # Chủ đề để gửi số lượng xe từ đường số 2
stream_url = "http://172.20.10.4:81/stream"  # Địa chỉ luồng video từ camera đặt ở đường 2

# Kết nối MQTT
client = mqtt.Client()
client.connect(broker, port)

# Biến lưu số lượng xe cũ để kiểm tra thay đổi
previous_vehicles = -1

# Hàm xử lý từng khung hình từ camera
def process_frame(frame):
    global previous_vehicles  # Dùng biến toàn cục để so sánh dữ liệu cũ và mới

    # Bước 1: Chuyển hình sang ảnh xám
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

    # Bước 2: Làm mờ ảnh để loại bỏ nhiễu
    blurred = cv2.GaussianBlur(gray, (5, 5), 0)

    # Bước 3: Dò biên bằng Canny
    edges = cv2.Canny(blurred, 50, 150)

    # Bước 4: Tìm các đường viền
    contours, _ = cv2.findContours(edges, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

    vehicles = 0  # Khởi tạo biến đếm xe

    # Duyệt từng contour để phát hiện xe
    for contour in contours:
        # Làm mượt contour thành hình đa giác
        approx = cv2.approxPolyDP(contour, 0.04 * cv2.arcLength(contour, True), True)

        # Kiểm tra nếu là hình chữ nhật và có diện tích đủ lớn
        if len(approx) == 4 and cv2.contourArea(contour) > 100:
            vehicles += 1  # Tăng biến đếm
            # Vẽ khung xanh quanh xe
            cv2.drawContours(frame, [approx], -1, (0, 255, 0), 2)

    # Nếu số lượng xe thay đổi, gửi lên MQTT
    if vehicles != previous_vehicles:
        client.publish(topic, vehicles)
        previous_vehicles = vehicles

    # Hiển thị số lượng xe trên khung hình
    cv2.putText(frame, f"Road-2: {vehicles}", (10, 30),
                cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)

    return frame  # Trả lại hình đã xử lý

# Hàm chính – xử lý luồng video
def main():
    print("Connecting to camera...")

    # Kết nối tới camera stream từ đường 2
    cap = cv2.VideoCapture(stream_url)

    # Kiểm tra kết nối
    if not cap.isOpened():
        print("Unable to connect to camera stream.")
        return

    # Vòng lặp chính
    while True:
        ret, frame = cap.read()  # Đọc từng khung hình

        # Nếu lỗi đọc ảnh, thử lại sau 1 giây
        if not ret:
            print("Frame read failed. Retrying...")
            time.sleep(1)
            continue

        # Hiển thị khung hình đã xử lý
        cv2.imshow('Vehicles Detection', process_frame(frame))

        # Nhấn 'q' để thoát
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    # Giải phóng tài nguyên sau khi kết thúc
    cap.release()
    cv2.destroyAllWindows()
    client.disconnect()

# Chạy chương trình nếu được gọi trực tiếp
if __name__ == "__main__":
    main()
