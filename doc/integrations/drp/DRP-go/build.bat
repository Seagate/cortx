set GOARCH=amd64
set GOOS=windows
go build -o drpclient.exe main.go
set GOOS=freebsd
go build -o drpclient.x main.go
