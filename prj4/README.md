# Project 4 · Dynamic Memory Allocator

`mm.c`에 동적 메모리 할당기 `mm_init`, `mm_malloc`, `mm_free`, `mm_realloc`을 구현했습니다.
블록의 헤더·푸터에 크기와 할당 상태를 기록하고, 크기별 분리 가용 리스트에서 적합한 블록을 찾습니다.
해제 시 인접 가용 블록을 병합하고, 할당 시 남는 공간을 분할합니다.

`mdriver.c`, `memlib.*`, 시간 측정 파일과 `traces/`는 수업 제공 평가 도구입니다.
`mm.c`를 수업 제공 드라이버로 빌드하고 평가합니다.

## 빌드와 실행

이 과제는 32비트 주소 크기를 기준으로 작성되었습니다. Ubuntu에서는 32비트 개발 패키지가 필요합니다.

```bash
sudo apt install build-essential gcc-multilib libc6-dev-i386
make
./mdriver -V
```

Makefile의 `-m32`를 유지해야 합니다. 포인터 크기가 바뀌는 64비트 빌드로 단순 대체하면
가용 리스트의 메모리 배치가 달라집니다.

## 문서와 검증

- [과제 설명](docs/assignment.pdf)
- [제출 보고서](docs/document.pdf)
- 빌드 확인(2026-10-08): 32비트 libc 헤더가 없는 환경에서 중단되었습니다.
  `gcc-multilib`와 `libc6-dev-i386` 설치 후 빌드 및 드라이버 실행이 필요합니다. 성능 점수는 미측정입니다.
