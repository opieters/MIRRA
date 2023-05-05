FROM python:3.7
SHELL ["/bin/bash", "-c"]

WORKDIR .
COPY . /app/ 

RUN set -x \ 
    && pip install  --no-cache-dir -r /app/requirements.txt 

CMD [ "python", "-u", "./app/mqtt_parser.py" ]