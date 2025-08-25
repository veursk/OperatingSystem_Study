/*
 * 프로세스 생성 실습 코드 (Process Creation Demo)
 * 
 * 이 프로그램은 Windows와 Unix 계열 시스템에서 
 * 프로세스를 생성하는 방법을 보여줍니다.
 * 
 * Windows: CreateProcess API 사용
 * Unix/Linux: fork + exec 조합 사용
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #include <windows.h>
  #include <process.h>   // _getpid() 함수용
#else
  #include <unistd.h>    // fork(), execvp(), getpid(), sleep() 함수용
  #include <sys/wait.h>  // waitpid() 함수용
#endif

// 전역 변수: 현재 프로세스가 자식인지, 몇 번째 자식인지 저장
static int is_child = 0;    // 1이면 자식 프로세스, 0이면 부모 프로세스
static int child_idx = 0;   // 자식 프로세스의 인덱스 번호 (1, 2, ...)

/*
 * 명령행 인수 파싱 함수
 * 
 * 프로그램이 자기 자신을 다시 실행할 때 사용하는 인수들:
 * --child: 이 프로세스가 자식 프로세스임을 나타냄
 * --id=N: 이 자식 프로세스의 인덱스 번호
 * 
 * 예: ./proc_demo --child --id=1
 */
static void parse_args(int argc, char** argv) {
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--child") == 0) {
      is_child = 1;  // 자식 모드로 설정
    } else if (strncmp(argv[i], "--id=", 5) == 0) {
      // "--id=" 다음 문자들을 숫자로 변환
      child_idx = atoi(argv[i] + 5);
    }
  }
}

/*
 * 자식 프로세스가 수행할 작업
 * 
 * 이 함수는 자식 프로세스로 실행될 때만 호출됩니다.
 * 자신의 PID를 출력하고, 1초간 대기한 후 종료합니다.
 */
static void child_work(void) {
#ifdef _WIN32
  // Windows에서 현재 프로세스의 PID 얻기
  int pid = (int)_getpid();
  printf("[child #%d] pid=%d: hello! working for 1s...\n", child_idx, pid);
  
  // Windows Sleep: 밀리초 단위 (1000ms = 1초)
  Sleep(1000);
  
  printf("[child #%d] done.\n", child_idx);
  
  // Windows에서 프로세스 종료 (종료 코드 = 자식 인덱스)
  // ExitProcess()는 즉시 프로세스를 종료시킴
  ExitProcess((UINT)child_idx);
  
#else
  // Unix/Linux에서 현재 프로세스의 PID와 부모 PID 얻기
  int pid = (int)getpid();   // 현재 프로세스 ID
  int ppid = (int)getppid(); // 부모 프로세스 ID
  printf("[child #%d] pid=%d ppid=%d: hello! working for 1s...\n", child_idx, pid, ppid);
  
  // Unix sleep: 초 단위 (1 = 1초)
  sleep(1);
  
  printf("[child #%d] done.\n", child_idx);
  
  // Unix에서 프로세스 종료 (종료 코드 = 자식 인덱스)
  // _exit()는 즉시 프로세스를 종료시킴 (cleanup 없이)
  _exit(child_idx);
#endif
}

/*
 * 메인 함수
 * 
 * 이 프로그램은 두 가지 모드로 실행됩니다:
 * 1. 부모 모드: 처음 실행될 때 (인수 없음)
 * 2. 자식 모드: 부모가 자식을 생성할 때 (--child --id=N 인수와 함께)
 */
int main(int argc, char** argv) {
  // 명령행 인수를 파싱하여 현재 모드 결정
  parse_args(argc, argv);

  // 자식 프로세스 모드인 경우
  if (is_child) {
    child_work();  // 자식 작업 수행
    return 0;      // 실제로는 child_work()에서 _exit()로 종료되므로 여기까지 오지 않음
  }

  // ==================== 부모 프로세스 모드 ====================
  printf("[parent] starting. (this is the terminal)\n");

#ifdef _WIN32
  // ==================== Windows 프로세스 생성 ====================
  
  // 1. 현재 실행 파일의 전체 경로를 얻기
  char self[MAX_PATH];
  GetModuleFileNameA(NULL, self, MAX_PATH);  // 현재 프로그램의 전체 경로
  
  printf("[parent] My executable path: %s\n", self);

  // 2개의 자식 프로세스를 순차적으로 생성
  for (int i = 1; i <= 2; ++i) {
    printf("\n[parent] Creating child process #%d...\n", i);
    
    // 2. 자식 프로세스 실행을 위한 명령행 생성
    // 형식: "프로그램경로" --child --id=번호
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "\"%s\" --child --id=%d", self, i);
    printf("[parent] Command: %s\n", cmd);

    // 3. Windows 프로세스 생성을 위한 구조체 초기화
    STARTUPINFOA si;           // 시작 정보 (창 설정 등)
    PROCESS_INFORMATION pi;    // 생성된 프로세스 정보 (PID, 핸들 등)
    
    // 구조체를 0으로 초기화 (매우 중요!)
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);  // 구조체 크기 설정

    // 4. CreateProcess API로 새 프로세스 생성
    BOOL ok = CreateProcessA(
      NULL,         // lpApplicationName: NULL이면 cmd에서 실행파일 찾음
      cmd,          // lpCommandLine: 실행할 명령행
      NULL,         // lpProcessAttributes: 프로세스 보안 속성 (기본값)
      NULL,         // lpThreadAttributes: 스레드 보안 속성 (기본값)
      FALSE,        // bInheritHandles: 핸들 상속 여부 (FALSE = 상속 안 함)
      0,            // dwCreationFlags: 생성 플래그 (기본값)
      NULL,         // lpEnvironment: 환경변수 (부모와 동일)
      NULL,         // lpCurrentDirectory: 작업 디렉토리 (부모와 동일)
      &si,          // lpStartupInfo: 시작 정보
      &pi           // lpProcessInformation: 생성된 프로세스 정보 (출력)
    );

    // 5. 프로세스 생성 결과 확인
    if (!ok) {
      DWORD e = GetLastError();  // Windows 에러 코드 얻기
      printf("[parent] CreateProcess failed (error code: %lu)\n", e);
      continue;  // 다음 자식 프로세스 생성 시도
    }

    printf("[parent] Successfully spawned child #%d (pid=%lu)\n", i, pi.dwProcessId);

    // 6. 자식 프로세스 종료 대기
    printf("[parent] Waiting for child #%d to finish...\n", i);
    
    // WaitForSingleObject 함수는 지정된 객체(여기서는 프로세스)가 신호 상태가 될 때까지 대기
    // - 첫 번째 매개변수 (pi.hProcess): 대기할 객체의 핸들
    // - 두 번째 매개변수 (INFINITE): 대기 시간 (밀리초)
    //   * INFINITE: 무한정 대기
    //   * 0: 즉시 반환
    //   * 양수: 지정된 밀리초만큼 대기
    // 반환값:
    // - WAIT_OBJECT_0: 객체가 신호 상태가 됨 (프로세스가 종료됨)
    // - WAIT_TIMEOUT: 제한 시간 초과
    // - WAIT_FAILED: 오류 발생
    WaitForSingleObject(pi.hProcess, INFINITE);  // 자식 프로세스가 종료될 때까지 무한정 대기
    
    // 7. 자식 프로세스의 종료 코드 확인
    DWORD ec = 0;
    GetExitCodeProcess(pi.hProcess, &ec);
    printf("[parent] Child #%d exited with code %lu\n", i, ec);

    // 8. 핸들 정리 (메모리 누수 방지)
    CloseHandle(pi.hThread);   // 스레드 핸들 닫기
    CloseHandle(pi.hProcess);  // 프로세스 핸들 닫기
  }

#else
  // ==================== Unix/Linux 프로세스 생성 ====================
  
  printf("[parent] My PID: %d\n", getpid());
  
  // 2개의 자식 프로세스를 순차적으로 생성
  for (int i = 1; i <= 2; ++i) {
    printf("\n[parent] Creating child process #%d...\n", i);
    
    // 1. fork() 시스템 콜로 프로세스 복제
    // fork()는 현재 프로세스를 완전히 복사하여 새로운 프로세스 생성
    pid_t pid = fork();
    
    // 2. fork() 결과 확인
    if (pid < 0) {
      // fork() 실패 (음수 반환)
      perror("[parent] fork failed");
      continue;  // 다음 자식 프로세스 생성 시도
    }
    
    if (pid == 0) {
      // ========== 자식 프로세스 영역 ==========
      // fork() 후 자식 프로세스에서는 pid가 0으로 반환됨
      
      printf("[child #%d] I'm the child! My PID: %d, Parent PID: %d\n", 
             i, getpid(), getppid());
      
      // 3. exec 계열 함수로 새로운 프로그램 실행
      // execvp()는 현재 프로세스 이미지를 새로운 프로그램으로 교체
      char idarg[16];
      snprintf(idarg, sizeof(idarg), "--id=%d", i);
      
      // 실행할 프로그램의 인수 배열 (NULL로 끝나야 함)
      char *args[] = { 
        argv[0],    // 프로그램 이름 (자기 자신)
        "--child",  // 자식 모드 플래그
        idarg,      // 자식 인덱스 (--id=1, --id=2, ...)
        NULL        // 배열 끝 표시
      };
      
      printf("[child #%d] Executing: %s %s %s\n", i, args[0], args[1], args[2]);
      
      // execvp() 실행: 성공하면 이 지점으로 돌아오지 않음
      execvp(args[0], args);
      
      // execvp()가 실패한 경우에만 여기 도달
      perror("[child] execvp failed");
      _exit(127);  // 표준 실패 코드로 즉시 종료
      
    } else {
      // ========== 부모 프로세스 영역 ==========
      // fork() 후 부모 프로세스에서는 자식의 PID가 반환됨
      
      printf("[parent] Successfully created child #%d (pid=%d)\n", i, pid);
      
      // 4. 자식 프로세스 종료 대기
      printf("[parent] Waiting for child #%d to finish...\n", i);
      int status = 0;
      waitpid(pid, &status, 0);  // 특정 자식 프로세스 대기
      
      // 5. 자식 프로세스 종료 상태 분석
      if (WIFEXITED(status)) {
        // 정상 종료: exit() 또는 return으로 종료
        int exit_code = WEXITSTATUS(status);
        printf("[parent] Child #%d exited normally with code %d\n", i, exit_code);
      } else if (WIFSIGNALED(status)) {
        // 시그널에 의한 종료: 강제 종료 등
        int signal_num = WTERMSIG(status);
        printf("[parent] Child #%d was killed by signal %d\n", i, signal_num);
      } else {
        // 기타 종료 상황
        printf("[parent] Child #%d terminated with status 0x%x\n", i, status);
      }
    }
  }
#endif

  // ==================== 부모 프로세스 종료 ====================
  printf("\n[parent] All child processes completed successfully!\n");
  printf("[parent] Parent process terminating...\n");
  return 0;  // 프로그램 정상 종료
}

/*
 * 실행 예시:
 * 
 * Windows:
 *   gcc -o proc_demo.exe proc_demo.c
 *   ./proc_demo.exe
 * 
 * Linux/Unix:
 *   gcc -o proc_demo proc_demo.c
 *   ./proc_demo
 * 
 * 예상 출력:
 *   [parent] starting. (this is the terminal)
 *   [parent] My executable path: /path/to/proc_demo
 *   [parent] Creating child process #1...
 *   [child #1] pid=1234: hello! working for 1s...
 *   [child #1] done.
 *   [parent] Child #1 exited with code 1
 *   [parent] Creating child process #2...
 *   [child #2] pid=1235: hello! working for 1s...
 *   [child #2] done.
 *   [parent] Child #2 exited with code 2
 *   [parent] All child processes completed successfully!
 */
