FROM alpine:3.20
RUN apk add --no-cache bash curl
COPY ops/tests/run.sh /run.sh
RUN chmod +x /run.sh
ENTRYPOINT ["/run.sh"]
