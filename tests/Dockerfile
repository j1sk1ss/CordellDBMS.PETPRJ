# docker build -t simulator .
# docker run -d -p 7777:7777 -v /Users/nikolaj/Documents/Repositories/CordellDBMS.EXMPL/builds:/app --name sim_container simulator

FROM debian:bullseye-slim

RUN apt-get update && apt-get install -y \
    libpthread-stubs0-dev \
    libssl-dev \
    curl \
    inotify-tools \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY cdbms_x86-64 /app/

EXPOSE 7777

CMD ["./cdbms_x86-64"]
