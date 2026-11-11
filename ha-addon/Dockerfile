FROM node:22-bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends python3 libwebp7 libwebpmux3 libwebpdemux2 tzdata ca-certificates curl \
 && curl -fsSL https://github.com/tidbyt/pixlet/releases/download/v0.34.0/pixlet_0.34.0_linux_amd64.tar.gz | tar -xz -C /usr/local/bin pixlet \
 && apt-get purge -y curl && apt-get autoremove -y && rm -rf /var/lib/apt/lists/*
ENV TZ=America/New_York
COPY run.sh /run.sh
RUN chmod +x /run.sh
CMD ["/run.sh"]
