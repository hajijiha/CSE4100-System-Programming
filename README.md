# CSE4100 · 시스템프로그래밍

자료구조 라이브러리, Unix 셸, 동시성 서버, 동적 메모리 할당기를 구현한 시스템프로그래밍 프로젝트입니다.

| 항목 | 내용 |
|---|---|
| 학교 | 서강대학교 |
| 학기 | 2025-1 |
| 과목코드 | CSE4100 |
| 개발 환경 | C / Make |
| 공통 자료 | [강의계획서](docs/syllabus.pdf) |

## 프로젝트

| 순서 | 프로젝트 | 구현 내용 | 과제 자료 |
|---|---|---|---|
| [prj1](prj1/README.md) | 자료구조 라이브러리 | 리스트·해시·비트맵 명령 처리 | [과제 설명](prj1/docs/assignment.pdf) |
| [prj2](prj2/README.md) | Unix Shell | 명령 실행, 파이프, 백그라운드 작업 제어 | [과제 설명](prj2/docs/assignment.pdf) |
| [prj3](prj3/README.md) | Concurrent Stock Server | I/O 다중화와 스레드 기반 동시 처리 | [과제 설명](prj3/docs/assignment.pdf) |
| [prj4](prj4/README.md) | Dynamic Memory Allocator | 분리 가용 리스트를 이용한 malloc/free/realloc | [과제 설명](prj4/docs/assignment.pdf) |

## 저장소 구조

```text
CSE4100-System-Programming/
├── README.md
├── docs/syllabus.pdf
├── prj1/  # 자료구조 라이브러리
├── prj2/  # Unix Shell
├── prj3/  # Concurrent Stock Server
├── prj4/  # Dynamic Memory Allocator
```

## 자료 출처

프로젝트에는 구현 소스와 수업 제공 스켈레톤·테스트 도구가 포함됩니다.
수업 제공 코드와 도구의 출처·라이선스는 각 원본 파일의 표기를 따릅니다.

[빌드 및 테스트](docs/verification.md)
