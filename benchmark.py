import subprocess
import time
import os

DB_FILENAME = "benchmark.db"
EXECUTABLE = "./db"
ROW_COUNT = 1000

if os.path.exists(DB_FILENAME):
    os.remove(DB_FILENAME)

print(f" Benchmarking {EXECUTABLE}...")

commands = []
for i in range(ROW_COUNT):
    commands.append(f"insert {i} user{i} person{i}@example.com")
commands.append(".exit")
input_data = "\n".join(commands) + "\n"

start_time = time.time()

# stderr(에러 메시지)를 캡처하도록 설정
process = subprocess.Popen(
    [EXECUTABLE, DB_FILENAME],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE, # 에러 캡처
    text=True
)

stdout, stderr = process.communicate(input=input_data)
end_time = time.time()

# 결과 출력
if process.returncode != 0:
    print("\nCRASH DETECTED (프로그램이 비정상 종료되었습니다)")
    print(f"Return Code: {process.returncode}")
    print(f"Error Message:\n{stderr}") # 여기에 'Segmentation fault'가 뜰 겁니다
else:
    print(f"Success! Time: {end_time - start_time:.4f}s")
    print(f"TPS: {ROW_COUNT / (end_time - start_time):.2f}")