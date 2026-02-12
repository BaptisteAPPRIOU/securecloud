# Service-to-Service mTLS (Optional)

This project now defaults to:

- External traffic terminated at `gateway` over HTTPS.
- Internal services reachable only on the private Docker network.

## Current Status

Service-to-service mTLS is **not enabled by default**.

Reason:

- Internal upstream calls currently use plain TCP HTTP from gateway to services.
- The upstream proxy path does not yet include native TLS client support for all upstream types.

## Recommended Rollout

1. Add per-service sidecar TLS proxies (or a service mesh) for `auth`, `files`, `messaging`, `deploy`.
2. Issue workload certs from an internal CA (SPIFFE/SPIRE, cert-manager, Vault PKI, or corporate PKI).
3. Route gateway upstream addresses to local mTLS egress proxies.
4. Enforce peer cert validation and SAN/identity checks on each service egress/ingress pair.
5. Rotate certs automatically and monitor handshake/verification failures.

## Practical Next Step

Use this sequence to move safely:

1. Keep internal network-only exposure (already in place).
2. Enforce trusted certs on gateway external ingress (already in place).
3. Introduce mTLS on one upstream (`auth`) first, validate, then expand to remaining services.
