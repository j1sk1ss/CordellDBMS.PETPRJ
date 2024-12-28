FROM ubuntu:latest

RUN apt-get update && apt-get install -y \
    build-essential \
    && apt-get clean

WORKDIR /cdbms_env
COPY ./builds /cdbms_env

EXPOSE 7777

CMD ["bash"]