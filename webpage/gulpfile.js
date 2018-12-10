const fs = require('fs');
const path = require('path');

const gulp = require('gulp');
const rename = require('gulp-rename');
const connect = require('gulp-connect');
const cheerio = require('gulp-cheerio');
const minifyhtml = require('gulp-minify-html');

const rollup = require("rollup");
const uglifyjs = require('uglify-es');

const postcss = require('postcss');
const autoprefixer = require('autoprefixer');
const uncss = require('uncss');
const uglifycss = require('uglifycss');


var cache = {};

gulp.task('connect', function() {
    connect.server({
        root: 'publish',
        port: 3000
    });
});

function inlinecss($, file, done) {
    $('link')
        .filter(function() {
            return $(this).attr("rel") === "stylesheet" && $(this).attr("inline") !== undefined;
        })
        .each(function() {
            var cachekey = "inlinecss:"+$(this).attr("cachekey");

            var data = null;
            if ($(this).attr("cachekey") !== undefined && cachekey in cache) {
                data = cache[cachekey];
            } else {
                data = fs.readFileSync("./build"+$(this).attr("href"), "utf8");
                if ($(this).attr("cachekey") !== undefined) cache[cachekey] = data;
            }

            $(this).replaceWith("<style" +
                ($(this).attr("clean") !== undefined? ' clean' : '') +
                ($(this).attr("minify") !== undefined? ' minify' : '') +
                ($(this).attr("cachekey") !== undefined? ' cachekey="'+$(this).attr("cachekey")+'"' : '') +
                ">"+data+"</style>");
        });

    done();
}

function inlinejs($, file, done) {
    $('script')
        .filter(function() {
            return $(this).attr("inline") !== undefined;
        })
        .each(function() {
            var src = $(this).attr("src");
            var cachekey = "inlinejs:"+$(this).attr("cachekey");

            var data = null;
            if ($(this).attr("cachekey") !== undefined && cachekey in cache) {
                data = cache[cachekey];
            } else {
                data = fs.readFileSync(path.join("./build", src), "utf8");
                if ($(this).attr("cachekey") !== undefined) cache[cachekey] = data;
            }

            $(this).attr("relative", src);
            $(this).removeAttr("src");
            $(this).removeAttr("inline");
            $(this).text(data);
        });
    done();
}

function compilejs($, file, done) {
    Promise.all(
        $('script')
            .filter(function() {
                return $(this).attr("type") === "module" && $(this).attr("es6") !== undefined;
            })
            .map(function() {
                var tag = $(this);
                var cachekey = "compilejs:"+tag.attr("cachekey");

                if (tag.attr("cachekey") !== undefined && cachekey in cache) {
                    tag.text(cache[cachekey]);
                    tag.removeAttr("type");
                    tag.removeAttr("es6");
                    return 0;

                } else {
                    return rollup.rollup({
                        input: 'inline-script',
                        plugins: [
                            {
                                name: 'inline-script',
                                resolveId: function(importee) { return importee === 'inline-script'? importee : null; },
                                load: function(id) { return id === 'inline-script'? tag.text() : null; }
                            },
                            {
                                resolveId: function(importee) { return importee.startsWith('/js/')? path.join('./build', importee) : null; }
                            },
                            {
                                resolveId: function(importee) {
                                    var relative = tag.attr("relative");
                                    if (relative === undefined || relative === '') return null;
                                    var filepath = path.join('./build', path.dirname(relative), importee);
                                    return fs.existsSync(filepath)? filepath : null;
                                }
                            },
                            require("rollup-plugin-babel")({
                                presets: [[require("@babel/preset-env"), {modules: false}]],
                            })
                        ]
                    }).then(function(bundle) {
                        return bundle.generate({
                            format: 'iife', name: 'bundle'
                        });
                    }).then(function(result) {
                        tag.text(result.code);
                        tag.removeAttr("type");
                        tag.removeAttr("es6");

                        if (tag.attr("cachekey") !== undefined) {
                            cache[cachekey] = result.code;
                        }
                    });
                }
            }).toArray()
    ).then(function() {
        done();
    });
}

function cleancss($, file, done) {
    Promise.all(
        $('style')
            .filter(function() {
                return $(this).attr("clean") !== undefined;
            })
            .map(function() {
                var tag = $(this);
                var cachekey = "cleancss:"+tag.attr("cachekey");

                if (tag.attr("cachekey") !== undefined && cachekey in cache) {
                    tag.text(cache[cachekey]);
                    tag.removeAttr("clean");

                } else {
                    var data = tag.text();
                    tag.text('');

                    return postcss([
                        uncss.postcssPlugin({
                            htmlroot: './build',
                            html: $.html().replace(/<script.*?<\/script>/gms, ''),
                            ignore: [
                                /\.navbar-menu\.is-active/,
                                /\.navbar-burger\.is-active/,
                                /\.fd-.*\.is-active/
                            ]
                        }),
                        autoprefixer({browsers: ['> 1%', 'last 2 versions']})
                    ])      
                    .process(data, {from: undefined})
                    .then(function(result) {
                        tag.text(result.css);
                        tag.removeAttr("clean");

                        if (tag.attr("cachekey") !== undefined) {
                            cache[cachekey] = result.css;
                        }
                    })
                }
            }).toArray()
    ).then(function() {
        done();
    });
}

function cleanjs($, file, done) {
    $('script')
        .filter(function() {
            return $(this).attr("relative") !== undefined;
        })
        .each(function() {
            $(this).removeAttr("relative");
        });
    done();
}

function minifycss($, file, done) {
    $('style')
        .filter(function() {
            return $(this).attr("minify") !== undefined;
        })
        .each(function() {
            var cachekey = "minifycss:"+$(this).attr("cachekey");

            if ($(this).attr("cachekey") !== undefined && cachekey in cache) {
                $(this).text(cache[cachekey]);

            } else {
                var text = uglifycss.processString($(this).text());
                $(this).text(text);

                if ($(this).attr("cachekey") !== undefined) {
                    cache[cachekey] = text;
                }
            }

            $(this).removeAttr("minify");
        });
    done();
}

function minifyjs($, file, done) {
    $('script')
        .filter(function() {
            return $(this).attr("minify") !== undefined;
        })
        .each(function() {
            var cachekey = "minifyjs:"+$(this).attr("cachekey");

            if ($(this).attr("cachekey") !== undefined && cachekey in cache) {
                $(this).text(cache[cachekey]);

            } else {
                var js = uglifyjs.minify($(this).text());
                if (js.error !== undefined) console.log(js.error);
                if (js.code !== undefined) {
                    var text = js.code;
                    $(this).text(text);

                    if ($(this).attr("cachekey") !== undefined) {
                        cache[cachekey] = text;
                    }
                }
            }

            $(this).removeAttr("minify");
        });
    done();
}

function cachekeyclean($, file, done) {
    $('style')
        .filter(function() { return $(this).attr("cachekey") !== undefined; })
        .each(function() { $(this).removeAttr("cachekey"); });

    $('script')
        .filter(function() { return $(this).attr("cachekey") !== undefined; })
        .each(function() { $(this).removeAttr("cachekey"); });

    done();
}

gulp.task('html', function() {
    return gulp.src('./build/**/*.html')
        .pipe(cheerio(inlinecss))
        .pipe(cheerio(inlinejs))
        .pipe(cheerio(compilejs))
        .pipe(cheerio(cleancss))
        .pipe(cheerio(cleanjs))
        .pipe(cheerio(minifycss))
        .pipe(cheerio(minifyjs))
        .pipe(cheerio(cachekeyclean))
        .pipe(minifyhtml())
        .pipe(rename(function (path) {
            if (path.basename != 'index' && path.extname == '.html') {
                path.extname = '';
            }
        }))
        .pipe(gulp.dest('./publish'))
});

gulp.task('images', function() {
    return gulp.src('./build/img/*.png')
        .pipe(gulp.dest('./publish/img'))
});

gulp.task('postprocess', ['html', 'images']);
