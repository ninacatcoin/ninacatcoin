# Multistage docker build, requires docker 17.05

# builder stage
FROM ubuntu:20.04 AS builder

RUN set -ex && \
    apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get --no-install-recommends --yes install \
        build-essential \
        ca-certificates \
        cmake \
        curl \
        git \
        pkg-config

WORKDIR /src
COPY . .

ARG NPROC
RUN set -ex && \
    git submodule init && git submodule update && \
    rm -rf build && \
    if [ -z "$NPROC" ] ; \
    then make -j$(nproc) depends target=x86_64-linux-gnu ; \
    else make -j$NPROC depends target=x86_64-linux-gnu ; \
    fi

# runtime stage
FROM ubuntu:20.04

RUN set -ex && \
    apt-get update && \
    apt-get --no-install-recommends --yes install ca-certificates && \
    apt-get clean && \
    rm -rf /var/lib/apt
COPY --from=builder /src/build/x86_64-linux-gnu/release/bin /usr/local/bin/

# Create ninacatcoin user
RUN adduser --system --group --disabled-password ninacatcoin && \
	mkdir -p /wallet /home/ninacatcoin/.ninacatcoin && \
	chown -R ninacatcoin:ninacatcoin /home/ninacatcoin/.ninacatcoin && \
	chown -R ninacatcoin:ninacatcoin /wallet

# Contains the blockchain
VOLUME /home/ninacatcoin/.ninacatcoin

# Generate your wallet via accessing the container and run:
# cd /wallet
# ninacatcoin-wallet-cli
VOLUME /wallet

EXPOSE 19080
EXPOSE 19081

# switch to user ninacatcoin
USER ninacatcoin

ENTRYPOINT ["ninacatcoind"]
CMD ["--p2p-bind-ip=0.0.0.0", "--p2p-bind-port=19080", "--rpc-bind-ip=0.0.0.0", "--rpc-bind-port=19081", "--non-interactive", "--confirm-external-bind"]

