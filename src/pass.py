import http.server
import socketserver
import json
import subprocess
import os

PORT = 5000

class EngineBridge(http.server.SimpleHTTPRequestHandler):
    # Cấu hình bỏ qua lỗi CORS chặn kết nối giữa Web và Backend
    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'POST, GET, OPTIONS')
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def do_POST(self):
        if self.path == '/solve':
            # Nhận phương trình từ Web truyền lên
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length)
            req = json.loads(post_data.decode('utf-8'))
            equation = req.get('equation', '')

            # Xác định tên file C đã được biên dịch (tuỳ thuộc vào Windows hay Mac/Linux)
            exe_name = "solver.exe" if os.name == 'nt' else "./solver"
            
            if not os.path.exists(exe_name) and not os.path.exists(exe_name.strip('./')):
                self.send_response(400)
                self.send_header('Access-Control-Allow-Origin', '*')
                self.end_headers()
                self.wfile.write(json.dumps({"error": f"Không tìm thấy '{exe_name}'. Bạn phải biên dịch file C trước!"}).encode())
                return

            try:
                # KÍCH HOẠT FILE C VÀ TRUYỀN PHƯƠNG TRÌNH VÀO
                process = subprocess.Popen(
                    [exe_name] if os.name == 'nt' else ["./solver"],
                    stdin=subprocess.PIPE,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                    encoding='utf-8'
                )
                # Đưa phương trình vào biến stdin (như cách bạn dùng hàm fgets trong C)
                stdout, stderr = process.communicate(input=equation + "\n", timeout=10)
                
                # Trả Output gốc nguyên văn từ C về cho Web
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.send_header('Access-Control-Allow-Origin', '*')
                self.end_headers()
                
                result_output = stdout if stdout else stderr
                self.wfile.write(json.dumps({"result": result_output}).encode())
                
            except Exception as e:
                self.send_response(500)
                self.send_header('Access-Control-Allow-Origin', '*')
                self.end_headers()
                self.wfile.write(json.dumps({"error": f"Lỗi thực thi C Engine: {str(e)}"}).encode())

# Tích hợp hàm GET để khi vào web nó tự mở file index.html
    def end_headers(self):
        self.send_header('Access-Control-Allow-Origin', '*')
        super().end_headers()

if __name__ == "__main__":
    with socketserver.TCPServer(("", PORT), EngineBridge) as httpd:
        print("===================================================")
        print("🚀 TRẠM TRUNG CHUYỂN C-ENGINE ĐANG CHẠY")
        print(f"👉 Mở trình duyệt và truy cập: http://localhost:{PORT}")
        print("Ấn Ctrl+C để tắt hệ thống.")
        print("===================================================")
        httpd.serve_forever()