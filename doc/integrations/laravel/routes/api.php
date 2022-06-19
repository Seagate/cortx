<?php

use Illuminate\Http\Request;
use Illuminate\Support\Facades\Route;
use Illuminate\Support\Facades\Storage;

/*
|--------------------------------------------------------------------------
| API Routes
|--------------------------------------------------------------------------
|
| Here is where you can register API routes for your application. These
| routes are loaded by the RouteServiceProvider within a group which
| is assigned the "api" middleware group. Enjoy building your API!
|
*/

// create object
Route::post('{path?}', function (Request $request, $path=Null) {
    $validator = Validator::make($request->all(), [
        'file' => 'required',
    ]);
    if ($validator->fails()) {
        return response()->json(['data' => [], 'message' => $validator->messages()->first(), 'status' => 'fail'], 401);
    }
    $file = $request->file('file');
    $file_name = $file->getClientOriginalName();
    return Storage::putFileAs($path, $file, $file_name);
})->where('path', '.*');

// read object
Route::get('download/{path?}', function (Request $request, $path=Null) {
    return Storage::download($path);
})->where('path', '.*');
Route::get('files/', function (Request $request) {
    return Storage::files('');
})->where('path', '.*');
Route::get('files/{path?}', function (Request $request, $path=Null) {
    return Storage::files($path);
})->where('path', '.*');
Route::get('all-files/', function (Request $request) {
    return Storage::allFiles('');
})->where('path', '.*');
Route::get('all-files/{path?}', function (Request $request, $path=Null) {
    return Storage::allFiles($path);
})->where('path', '.*');
Route::get('{path?}', function (Request $request, $path=Null) {
    return Storage::get($path);
})->where('path', '.*');

// delete object
Route::delete('{path?}', function (Request $request, $path=Null) {
    return Storage::delete($path);
})->where('path', '.*');
