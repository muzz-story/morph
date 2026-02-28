---
name: "Bug Report / 버그 제보"
about: "Report a bug to help us improve / 버그를 제보해 주세요"
title: "[BUG] "
labels: bug
assignees: ""
---

<!-- English below / 한국어는 아래에 -->

## Describe the bug
A clear description of what the bug is.

## Request that caused the bug
```
# URL or curl command
GET /morph/myservice/500x500/images/photo.jpg
```

## Expected behavior
What you expected to happen.

## Actual behavior
What actually happened. Include the HTTP status code and any error messages.

## Debug log
Enable `morph_debug on;` in `nginx.conf` and attach the relevant lines from `logs/morph_debug.log`.

```
# paste log here
```

## Environment
- OS:
- Nginx version:
- Libvips version:
- Build method: Docker / Native Linux

---

## 버그 설명
어떤 버그인지 간략히 설명해 주세요.

## 버그를 발생시킨 요청
```
# URL 또는 curl 명령어
GET /morph/myservice/500x500/images/photo.jpg
```

## 기대했던 결과
어떤 결과를 기대했는지 작성해 주세요.

## 실제 결과
실제로 발생한 일을 작성해 주세요. HTTP 상태 코드와 에러 메시지를 포함해 주세요.

## 디버그 로그
`nginx.conf`에서 `morph_debug on;`을 활성화하고 `logs/morph_debug.log`의 관련 내용을 첨부해 주세요.

```
# 로그를 여기에 붙여넣기
```

## 환경
- OS:
- Nginx 버전:
- Libvips 버전:
- 빌드 방법: Docker / Linux 단독 설치
