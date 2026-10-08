# CSE4100 · 시스템프로그래밍

자료구조 라이브러리에서 시작해 Unix 셸, 동시성 서버, 동적 메모리 할당기를 구현한 과제 모음입니다.

| 항목 | 내용 |
|---|---|
| 학교 | 서강대학교 |
| 학기 | 2025-1 |
| 과목코드 | CSE4100 |
| 개발 환경 | C / Make |
| 공통 자료 | [강의계획서](docs/syllabus.pdf) |

## 프로젝트

| 순서 | 프로젝트 | 구현 내용 |
|---|---|---|
| [prj1](prj1/README.md) | 자료구조 라이브러리 | 리스트·해시·비트맵 명령 처리 |
| [prj2](prj2/README.md) | Unix Shell | 명령 실행, 파이프, 백그라운드 작업 제어 |
| [prj3](prj3/README.md) | Concurrent Stock Server | I/O 다중화와 스레드 기반 동시 처리 |
| [prj4](prj4/README.md) | Dynamic Memory Allocator | 분리 가용 리스트를 이용한 malloc/free/realloc |

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

프로젝트별 README에서 구현 파일, 실행 명령, 관련 문서와 검증 범위를 확인할 수 있습니다.
학기와 과목코드는 해당 학기의 강의계획서를 기준으로 기록했습니다.

## 자료 출처

제출본과 수업 제공 스켈레톤·테스트 도구를 함께 정리했습니다. 제공 코드와 팀 코드의
저작권 표시를 유지하고, 직접 구현한 부분은 프로젝트별 README에 구분했습니다.

[정리 시 검증 기록](docs/verification.md)
