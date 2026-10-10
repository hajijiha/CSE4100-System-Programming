# Project 1 · 자료구조 라이브러리

리스트, 해시 테이블, 비트맵을 생성하고 명령어로 조작하는 C 프로그램입니다.

## 구현과 구조

- `main.c`: 명령 입력, 토큰 분리, 자료구조별 함수 호출과 메모리 정리.
- `list.c/.h`: 연결 리스트와 정렬·삽입·삭제·순회 연산.
- `hash.c/.h`: 해시 테이블의 삽입·검색·삭제·변환.
- `bitmap.c/.h`: 비트 설정·검사·검색·확장.
- `hex_dump.c/.h`, `debug.*`, `limits.h`, `round.h`: 수업에서 제공한 지원 코드.
- `tester/`: 수업 제공 입력과 예상 출력, 검증 스크립트.

## 빌드와 실행

Linux/WSL 환경에 GCC와 Make가 필요합니다. 이 디렉터리에서 실행합니다.

```bash
make
./testlib
```

예를 들어 `create list list0`, `list_push_back list0 10`, `dumpdata list0`, `quit`를 입력할 수 있습니다.

```bash
cd tester
bash prj1_tester.sh ../testlib
```

`list_shuffle`은 출력 순서가 임의이므로 제공 스크립트에서도 수동으로 확인하도록 되어 있습니다.

## 문서와 검증

- [과제 설명](docs/assignment.pdf)
- [제출 보고서](docs/document_20211605.docx)
- 빌드 확인: Ubuntu, `make` 통과 (2026-10-08).
- 제공 테스트 입력 38개는 예상 출력과 일치했습니다 (무작위 shuffle 제외).
