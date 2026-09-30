FROM rust:1.96-bookworm AS build
WORKDIR /app
COPY Cargo.toml Cargo.lock ./
COPY src ./src
COPY migrations ./migrations
RUN cargo build --release --locked

FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates curl && rm -rf /var/lib/apt/lists/* && useradd --system --uid 10001 api
COPY --from=build /app/target/release/td-project-api /usr/local/bin/td-project-api
USER api
STOPSIGNAL SIGINT
EXPOSE 8080
HEALTHCHECK --interval=10s --timeout=3s --start-period=10s CMD curl -fsS http://127.0.0.1:8080/health || exit 1
ENTRYPOINT ["td-project-api"]
