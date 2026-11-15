import base64, json, sys, httpx

def attempt(base, size):
    payload = base64.b64encode(b"\x00" * size).decode()
    body = {"user_id": 1, "mime": "image/png", "data_base64": payload}
    client = httpx.Client(base_url=base, timeout=30.0)
    try:
        resp = client.post("/api/upload-avatar", json=body)
        return f"status={resp.status_code} body={resp.text[:80]}"
    except Exception as exc:
        return f"exception: {type(exc).__name__}: {str(exc)[:80]}"
    finally:
        client.close()

for size in (900_000, 1_500_000, 2_100_000):
    print(f"--- payload {size/1024:.0f} KB ---")
    print("  C++   :", attempt("http://127.0.0.1:9101", size))
    print("  Python:", attempt("http://127.0.0.1:9100", size))
