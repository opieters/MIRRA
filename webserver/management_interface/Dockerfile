FROM tiangolo/uwsgi-nginx-flask:python3.7
SHELL ["/bin/bash", "-c"]

WORKDIR .
COPY . /app/ 

RUN set -x \ 
    && pip install -r /app/requirements.txt \
    && echo -e "[uwsgi]\nmodule = app\ncallable = app\nenable-threads = true" > /app/uwsgi.ini


# RUN set -x \ 
#     && pip install -r /app/app/requirements.txt \
#     && touch /app/app/__init__.py \
#     && echo -e "[uwsgi]\nmodule = app.app\ncallable = app" > /app/uwsgi.ini

### Installation commands
##  1. Build image from this dockerFile
# docker build -t gems_image .

##  2. Create a container from that image and run it
#  docker run -d --name gems_container -p 443:443 GEMS_image

## Optionals: debug with bash:
#  docker run -ti gems_image bash