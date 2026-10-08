# Project 2 · Unix Shell

사용자가 입력한 명령을 실행하는 셸을 단계별로 확장했습니다.

| 단계 | 구현 |
|---|---|
| `phase1/` | 기본 명령 실행과 내장 명령 |
| `phase2/` | 파이프로 연결한 명령 실행 |
| `phase3/` | 백그라운드 작업, jobs/fg/bg/kill, 시그널과 작업 제어 |

각 단계의 `shellex.c`가 구현 파일이며 `Makefile`로 `myshell`을 만듭니다.
제출본에서 생략되어 있던 `csapp.c/.h`는 해당 과제의 제공 스켈레톤에서 보충했습니다.

## 빌드와 실행

GCC, Make와 POSIX 프로세스·시그널 기능이 있는 Linux/WSL 터미널이 필요합니다.

```bash
cd phase3
make
./myshell
```

```text
ls -l
ls | wc -l
sleep 10 &
jobs
fg %1
quit
```

`phase1`, `phase2`도 해당 폴더에서 `make`와 `./myshell`로 실행합니다.
작업 제어는 대화형 터미널에서 확인하며 Ctrl-C와 Ctrl-Z는 포그라운드 작업에 전달됩니다.

## 문서와 검증

- [과제 설명](docs/assignment.pdf)
- [제출 보고서](docs/document.docx)
- 각 단계의 `README.txt`에 세부 구현 설명이 있습니다.
- 2026-10-08: 세 단계 모두 Ubuntu에서 빌드 통과. phase2에는 기존 컴파일 경고가 있습니다.
