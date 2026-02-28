# Security Policy

**English** | [한국어](#보안-정책)

---

## Supported Versions

This project is currently in active development. Security fixes are applied to the latest version on the `master` branch.

---

## Reporting a Vulnerability

Please **do not** open a public GitHub Issue for security vulnerabilities.

Instead, report vulnerabilities privately using [GitHub's private vulnerability reporting](../../security/advisories/new).

Include:
- A description of the vulnerability
- Steps to reproduce
- Potential impact
- A suggested fix if you have one

You can expect an acknowledgement within a few days and a fix or mitigation plan as soon as possible.

---

## Known Security Measures

| Threat | Mitigation |
|---|---|
| **SSRF** | Remote URLs are blocked if they resolve to localhost or private IP ranges (`127.x`, `10.x`, `192.168.x`, `::1`) |
| **Path Traversal** | Source paths containing `..` are rejected with `403 Forbidden` |
| **Cache Purge abuse** | `DELETE` requests are restricted to `127.0.0.1` by default; additional admin IPs must be explicitly configured in `nginx.conf` |
| **Unsafe formats** | Only JPEG, PNG, WebP, and GIF are accepted; unknown magic bytes return `415 Unsupported Media Type` |
| **Oversized GIFs** | Animated GIFs exceeding 200 megapixels fall back to static single-frame rendering |

---

## Scope

In-scope security issues:

- SSRF bypass via URL manipulation
- Path traversal to read arbitrary files
- Cache poisoning or arbitrary file write via crafted requests
- Remote code execution via malformed image data
- Authentication/authorization bypass for cache purge

Out of scope:

- Issues in upstream dependencies (Nginx, Libvips, libcurl) — please report those directly to their maintainers
- Denial-of-service via legitimate large images (use `width_max` / `height_max` directives to limit)

---
---

# 보안 정책

[English](#security-policy) | **한국어**

---

## 지원 버전

현재 활발히 개발 중인 프로젝트입니다. 보안 수정은 `master` 브랜치의 최신 버전에 반영됩니다.

---

## 취약점 제보

보안 취약점은 **공개 Issue로 올리지 말아 주세요.**

[GitHub 비공개 취약점 보고](../../security/advisories/new) 기능을 통해 제보해 주세요.

제보 시 다음 내용을 포함해 주시면 도움이 됩니다:
- 취약점 설명
- 재현 방법
- 예상 영향 범위
- 수정 방법 제안 (있을 경우)

가능한 빠르게 확인 후 회신드리겠습니다.

---

## 구현된 보안 조치

| 위협 | 대응 |
|---|---|
| **SSRF** | localhost 및 사설 IP 대역(`127.x`, `10.x`, `192.168.x`, `::1`)으로의 요청 차단 |
| **Path Traversal** | `..` 포함 경로 요청 시 `403 Forbidden` 반환 |
| **캐시 Purge 남용** | `DELETE` 요청은 기본적으로 `127.0.0.1`만 허용, 추가 IP는 `nginx.conf`에서 명시적으로 설정 필요 |
| **비정상 포맷** | JPEG, PNG, WebP, GIF만 허용, 알 수 없는 매직 바이트는 `415 Unsupported Media Type` 반환 |
| **대용량 GIF** | 200MP 초과 애니메이션 GIF는 첫 프레임 정적 이미지로 자동 전환 |

---

## 범위

**해당 보안 이슈:**

- URL 조작을 통한 SSRF 우회
- 임의 파일 읽기를 위한 Path Traversal
- 악의적인 요청을 통한 캐시 오염 또는 임의 파일 쓰기
- 잘못된 이미지 데이터를 통한 원격 코드 실행
- 캐시 Purge 인증/권한 우회

**해당 없는 이슈:**

- 업스트림 의존성(Nginx, Libvips, libcurl)의 취약점 — 해당 프로젝트에 직접 제보 바랍니다
- 정상적인 대용량 이미지로 인한 서비스 거부 — `width_max` / `height_max` 디렉티브로 제한 가능
