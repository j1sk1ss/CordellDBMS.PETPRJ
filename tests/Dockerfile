# docker build -t simulator .
# docker run --rm -d -p 7777:7777 -v /home/j1sk1ss/Desktop/CordellDBMS.PETPRJ/builds:/app --name sim_container simulator

FROM debian:bookworm-slim


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
