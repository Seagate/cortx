var http = require('http'),
  httpProxy = require('http-proxy')

var proxy = new httpProxy.createProxyServer({
  target: {
    // TODO change this to the cloudshare endpoint
    host: __LOTUS_RPC_ENDPOINT__,
    port: __LOTUS_PORT__,
  },
})

proxy.on('proxyReq', function (proxyReq, req, res, options) {
  res.setHeader('Access-Control-Allow-Origin', '*')
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type,Authorization')
  res.setHeader('Access-Control-Allow-Methods', 'GET,PATCH,POST,PUT,DELETE')

  if (proxyReq.method === 'OPTIONS') {
    res.statusCode = 200
    res.end()
    return
  }
})

var proxyServer = http.createServer(function (req, res) {
  proxy.web(req, res)
})

//
// Listen to the `upgrade` event and proxy the
// WebSocket requests as well.
//
proxyServer.on('upgrade', function (req, socket, head) {
  proxy.ws(req, socket, head)
})

process.on('uncaughtException', function (error) {
  console.log(error)
})

process.on('unhandledRejection', function (reason, p) {
  console.log(reason)
})

proxyServer.listen(8000)
