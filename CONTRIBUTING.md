# Contributing to Nginx Morph Module

**English** | [한국어](#한국어-기여-가이드)

Thank you for your interest in contributing! Any help — bug reports, feature ideas, or code — is welcome.

---

## Reporting Bugs

Open an [Issue](../../issues/new) and include:

- What you did (URL, config, options used)
- What you expected
- What actually happened (error log, HTTP response)
- Environment (OS, Nginx version, Libvips version)

The `morph_debug on;` directive enables detailed debug logging at `logs/morph_debug.log` — attaching it is very helpful.

---

## Suggesting Features

Open an [Issue](../../issues/new) with the label `enhancement`.
Describe the use case and expected behavior. If you have a URL option syntax proposal, include an example.

---

## Submitting Pull Requests

1. **Fork** the repository and create a branch from `master`
2. **Build and test** locally using Docker:
   ```bash
   cd docker
   docker-compose up --build
   ```
3. Keep changes focused — one fix or feature per PR
4. Make sure the build succeeds before opening the PR
5. Describe what the PR does and why in the PR description

---

## Build Environment

The recommended way to build and test is via Docker (Rocky Linux 9 base):

```bash
cd docker
docker-compose up --build
# Server runs on http://localhost:8080/morph/...
```

For a native Linux build, see the [README](README.en.md#linux-installation-without-docker).

---

## Code Style

- Language: **C++11**
- Follow the existing indentation and brace style in the source files
- Prefer explicit over clever — readability matters
- Nginx module conventions: use `ngx_log_error` for logging, `ngx_pcalloc` for pool allocation

---

## License

By contributing, you agree that your contributions will be licensed under the [Apache License 2.0](LICENSE).

---
---

# 한국어 기여 가이드

[English](#contributing-to-nginx-morph-module) | **한국어**

기여에 관심 가져주셔서 감사합니다! 버그 제보, 기능 제안, 코드 기여 모두 환영합니다.

---

## 버그 제보

[Issue](../../issues/new)를 열고 다음 내용을 포함해 주세요:

- 어떤 작업을 했는지 (사용한 URL, 설정, 옵션)
- 기대했던 결과
- 실제로 발생한 일 (에러 로그, HTTP 응답)
- 환경 정보 (OS, Nginx 버전, Libvips 버전)

`morph_debug on;` 디렉티브를 활성화하면 `logs/morph_debug.log`에 상세 디버그 로그가 기록됩니다. 로그를 함께 첨부해 주시면 큰 도움이 됩니다.

---

## 기능 제안

[Issue](../../issues/new)를 `enhancement` 레이블로 열어주세요.
사용 사례와 원하는 동작을 설명해 주세요. URL 옵션 문법 제안이 있다면 예시도 함께 작성해 주세요.

---

## Pull Request 제출

1. 저장소를 **Fork**하고 `master` 기반으로 브랜치를 생성
2. Docker로 **빌드 및 테스트**:
   ```bash
   cd docker
   docker-compose up --build
   ```
3. PR 하나에는 하나의 수정/기능만 포함
4. PR을 열기 전에 빌드가 성공하는지 확인
5. PR 설명에 무엇을, 왜 변경했는지 작성

---

## 빌드 환경

Docker를 사용하는 것을 권장합니다 (Rocky Linux 9 기반):

```bash
cd docker
docker-compose up --build
# 서버: http://localhost:8080/morph/...
```

Linux 단독 설치는 [README](README.md#docker-없이-단독-서버-세팅-법-linux)를 참고하세요.

---

## 코드 스타일

- 언어: **C++11**
- 기존 소스 파일의 들여쓰기와 중괄호 스타일을 따라주세요
- 영리한 코드보다 명확한 코드를 선호합니다
- Nginx 모듈 컨벤션: 로깅은 `ngx_log_error`, 메모리 할당은 `ngx_pcalloc` 사용

---

## 라이선스

기여하신 내용은 [Apache License 2.0](LICENSE)으로 배포됩니다.
