FROM gcc:latest

WORKDIR /context_switch_lab
COPY . .

RUN make

CMD ["./context_switch_lab"]
