PWD := $(shell pwd)
GOPATH := $(shell go env GOPATH)
LDFLAGS := $(shell go run buildscripts/gen-ldflags.go)

GOARCH := $(shell go env GOARCH)
GOOS := $(shell go env GOOS)

VERSION ?= $(shell git describe --tags)
TAG ?= "minio/minio:$(VERSION)"
BUILD_LDFLAGS := '$(LDFLAGS)'

all:
	@( echo "Target must be specified: x86 or arm" ; 	exit 1)

x86: set_x86 target

arm: set_arm target

set_x86:
	$(eval architecture := x86)
	$(shell ./gox86env.sh)

set_arm:
	$(eval architecture := arm)
	$(eval extra_flags := env GOOS=linux GOARCH=arm GOARM=7)
	$(shell ./goarmenv.sh)

target: build
	# Adding @() to any bash command removes the echo
	@( \
	if [ ! -f "minio" ]; \
	then \
		echo === New executable minio was not created ===; \
	fi)
	@( \
	if [ -f "minio" ]; \
	then \
		[ ! -d "./bin" ] && mkdir ./bin; \
		mv minio ./bin/s3kinetic.$(architecture); \
		echo === New executable minio was created: ./bin/s3kinetic.$(architecture) ===; \
	fi)

checks:
	@echo "Checking dependencies"
	@(env bash $(PWD)/buildscripts/checkdeps.sh)

libs:
	@( \
	if [ -d "lib" ]; \
	then \
		echo "Found libraries from the kinetic project in ./lib folder: skipping library copy step"; \
	else \
		echo "Copying libraries from kineticd"; \
		./cp_kinetic_libs.sh -a $(architecture); \
	fi)

getdeps:
	@mkdir -p ${GOPATH}/bin
	@which golint 1>/dev/null || (echo "Installing golint" && GO111MODULE=off go get -u golang.org/x/lint/golint)
ifeq ($(GOARCH),s390x)
	@which staticcheck 1>/dev/null || (echo "Installing staticcheck" && GO111MODULE=off go get honnef.co/go/tools/cmd/staticcheck)
else
	@which staticcheck 1>/dev/null || (echo "Installing staticcheck" && wget --quiet https://github.com/dominikh/go-tools/releases/download/2019.2.3/staticcheck_${GOOS}_${GOARCH}.tar.gz && tar xf staticcheck_${GOOS}_${GOARCH}.tar.gz && mv staticcheck/staticcheck ${GOPATH}/bin/staticcheck && chmod +x ${GOPATH}/bin/staticcheck && rm -f staticcheck_${GOOS}_${GOARCH}.tar.gz && rm -rf staticcheck)
endif
	@which misspell 1>/dev/null || (echo "Installing misspell" && GO111MODULE=off go get -u github.com/client9/misspell/cmd/misspell)

crosscompile:
	@(env bash $(PWD)/buildscripts/cross-compile.sh)

verifiers: getdeps vet fmt lint staticcheck spelling

vet:
	@echo "Running $@ check"
	@GO111MODULE=on go vet github.com/minio/minio/...

fmt:
	@echo "Running $@ check"
	@GO111MODULE=on gofmt -d cmd/
	@GO111MODULE=on gofmt -d pkg/

lint:
	@echo "Running $@ check"
	@GO111MODULE=on ${GOPATH}/bin/golint -set_exit_status github.com/minio/minio/cmd/...
	@GO111MODULE=on ${GOPATH}/bin/golint -set_exit_status github.com/minio/minio/pkg/...

staticcheck:
	@echo "Running $@ check"
	@GO111MODULE=on ${GOPATH}/bin/staticcheck github.com/minio/minio/cmd/...
	@GO111MODULE=on ${GOPATH}/bin/staticcheck github.com/minio/minio/pkg/...

spelling:
	@echo "Running $@ check"
	@GO111MODULE=on ${GOPATH}/bin/misspell -locale US -error `find cmd/`
	@GO111MODULE=on ${GOPATH}/bin/misspell -locale US -error `find pkg/`
	@GO111MODULE=on ${GOPATH}/bin/misspell -locale US -error `find docs/`
	@GO111MODULE=on ${GOPATH}/bin/misspell -locale US -error `find buildscripts/`
	@GO111MODULE=on ${GOPATH}/bin/misspell -locale US -error `find dockerscripts/`

# Builds minio, runs the verifiers then runs the tests.
check: test
test: verifiers build
	@echo "Running unit tests"
	@GO111MODULE=on CGO_ENABLED=0 go test -tags kqueue ./... 1>/dev/null

verify: build
	@echo "Verifying build"
	@(env bash $(PWD)/buildscripts/verify-build.sh)

coverage: build
	@echo "Running all coverage for minio"
	@(env bash $(PWD)/buildscripts/go-coverage.sh)

# Builds minio locally.
build: checks libs
	@echo "Building minio binary to './minio'"
	@GO111MODULE=on $(extra_flags) CGO_ENABLED=1 go build -x -tags kqueue --ldflags $(BUILD_LDFLAGS) -o $(PWD)/minio 1>/dev/null

docker: build
	@docker build -t $(TAG) . -f Dockerfile.dev

# Builds minio and installs it to $GOPATH/bin.
install: build
	@echo "Installing minio binary to '$(GOPATH)/bin/minio'"
	@mkdir -p $(GOPATH)/bin && cp -f $(PWD)/minio $(GOPATH)/bin/minio
	@echo "Installation successful. To learn more, try \"minio --help\"."

clean:
	@echo "Cleaning up all the generated files"
	@find . -name '*.test' | xargs rm -fv
	@find . -name '*~' | xargs rm -fv
	@rm -rvf minio
	@rm -rvf build
	@rm -rvf release
	@rm -rvf lib
	@rm -rvf bin
