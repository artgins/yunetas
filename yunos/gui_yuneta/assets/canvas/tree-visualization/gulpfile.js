var gulp = require('gulp')
var browserSync = require('browser-sync').create()
var sourcemaps = require('gulp-sourcemaps')
var babel = require('gulp-babel')
var concat = require('gulp-concat')

gulp.task('browser-sync', function() {
  browserSync.init({
    server: {
      baseDir: "./public"
    }
  })
})

gulp.task('html', function(done) {
  return gulp.src('src/**/*.html')
    .pipe(gulp.dest('public'))
})

gulp.task('js', function(done) {
  return gulp.src('src/**/*.js')
    .pipe(sourcemaps.init())
    .pipe(babel({
      presets: ['env']
    }))
    .pipe(concat('index.js'))
    .pipe(sourcemaps.write('.'))
    .pipe(gulp.dest('public'))
})

gulp.task('css', function(done) {
  return gulp.src('src/**/*.css')
    .pipe(gulp.dest('public'))
})

gulp.task('reload', function(done) {
  browserSync.reload()
  done()
})

gulp.task('default', ['browser-sync', 'js', 'css', 'html'], function() {
  gulp.watch('src/**/*.html', ['html', 'reload'])
  gulp.watch('src/**/*.js', ['js', 'reload'])
  gulp.watch('src/**/*.css', ['css', 'reload'])
})
