# Project 3 · Concurrent Stock Server

주식 목록을 조회하고 수량을 구매·판매하는 TCP 서버와 클라이언트입니다.
동시 요청 처리 방식을 두 가지로 구현했습니다.

| 폴더 | 방식 |
|---|---|
| `task1/` | `select` 기반 I/O 다중화 |
| `task2/` | POSIX 스레드 기반 동시 처리 |

`stockserver.c`에 서버와 공유 자료구조 관리가 있으며, `stockclient.c`는 대화형 클라이언트,
`multiclient.c`는 다중 클라이언트 도구, `stock.txt`는 초기 주식 데이터입니다.
CS:APP의 네트워크 지원 코드 `csapp.c/.h`와 과제 제공 클라이언트를 함께 보관합니다.

## 빌드와 실행

Linux/WSL에서 GCC, Make, pthread가 필요합니다.

```bash
cd task1
make
./stockserver 12345
```

다른 터미널에서 같은 폴더로 이동합니다.

```bash
./stockclient 127.0.0.1 12345
```

클라이언트 명령은 `show`, `buy <주식 ID> <수량>`, `sell <주식 ID> <수량>`, `exit`입니다.
`task2/`에서도 같은 명령을 사용하며 같은 포트를 동시에 쓰지 않습니다.

## 문서와 검증

- [과제 설명](docs/assignment.pdf)
- [제출 보고서](docs/document.pdf)
- 2026-10-08: 두 구현 모두 Ubuntu에서 서버·클라이언트 빌드 통과.
