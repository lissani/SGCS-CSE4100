# Phase 2: Redirection and Pipe
Phase 2에서는 사용자가 입력한 명령어에 대해 `|`(pipe) 연산자를 해석하여, 복수의 명령어를 파이프라인(pipeline) 으로 연결 실행할 수 있는 쉘 기능을 구현했습니다.

### 주요 기능
- 명령어 분리 및 파싱
    - `|` 기준으로 입력 명령어를 분할 (`strtok(cmdline, "|")`)
    - 각 명령어 토큰에 대해 공백 제거 및 quote-aware 인자 파싱 (`parse_args_quote_aware`)
    - 빈 명령어나 공백-only 명령어가 있을 경우 에러 출력
- 파이프 명령어 실행
    - 각 파이프 구간마다 `pipe()`, `fork()`를 호출
    - `dup2()`를 이용해 `stdin`, `stdout`을 pipe의 읽기/쓰기 단자로 재지정
    - 마지막 명령어까지 파이프로 연결 후, 모든 자식 `waitpid()`로 종료 대기
- 시그널 처리
    - 파이프라인 실행 중 `Ctrl+C` 시 모든 자식 프로세스에 `SIGINT` 전송
    - `SIGINT`를 받으면 현재 실행 중인 모든 자식 `kill()` 후 수거 (`waitpid()`)