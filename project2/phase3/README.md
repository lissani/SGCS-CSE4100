# Phase 3: Run Processes in Background
Phase 3에서는 기본 쉘에 잡 컨트롤(job control) 기능을 추가하여 백그라운드 프로세스 실행, 작업 제어 명령어(fg, bg, kill, jobs) 및 시그널 핸들링을 지원하도록 확장하였습니다.

### 주요 기능
- 백그라운드 실행
    - 명령어 끝에 &를 붙이면 백그라운드에서 실행
    - 실행 중인 프로세스의 PGID(Process Group ID)를 기준으로 jobs에 등록되고 출력
- jobs 명령어
    - 현재 백그라운드 및 정지된 작업 목록을 출력
    - +는 가장 최근 작업, -는 그 이전 작업을 의미
- fg, bg 명령어
    - %<job_id> 형식으로 특정 작업을 포그라운드/백그라운드로 전환
    - 인자 생략 시 current_job(+ 표시된 작업)이 기본값
- kill 명령어
    - %<job_id> 형식으로 특정 작업을 종료시킨다.
    - 내부적으로 SIGTERM을 해당 프로세스 그룹 전체에 전송