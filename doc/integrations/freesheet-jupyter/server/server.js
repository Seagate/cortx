// Require the framework and instantiate it
// const fastify = require('fastify')({ logger: true })
import * as Fastify from 'fastify'
import mp from '@fastify/multipart'
import cors from '@fastify/cors'
import { getBuckets, getObject, listObjects, putObject } from './cortx.js'

const fastify = Fastify.fastify({logger: true})
fastify.register(mp)
fastify.register(cors, { 
  origin: (origin, cb) => {
    const hostname = new URL(origin).hostname
    if(hostname === "localhost"){
      //  Request from localhost will pass
      cb(null, true)
      return
    }
    // Generate an error on other origins, disabling access
    cb(new Error("Not allowed"), false)
  }
})


// Run the server!
const start = async () => {
  try {
    await fastify.listen(3001)
  } catch (err) {
    fastify.log.error(err)
    process.exit(1)
  }
}

// Declare a route
fastify.get('/', async (request, reply) => {
  return { hello: 'world' }
})

fastify.get('/object/:bucket/:objectId', async (request, reply) => {
  const {params} = request
  console.log('get object', params.bucket, params.objectId)
  return await getObject(params.bucket, params.objectId)
})

fastify.get('/objects/:bucket', async (request, reply) => {
  const {params} = request
  console.log('list objects', params.bucket);
  return await listObjects(params.bucket)
})

fastify.get('/buckets', async (request, reply) => {
  console.log('get buckets')
  return await getBuckets()
})

fastify.post('/object/:bucket', async (request, reply) => {
  const {params} = request
  console.log('put object', params.bucket)
  const data = await request.file()

  return await putObject(params.bucket, data.filename, await data.toBuffer())
})


start()