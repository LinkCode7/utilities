mergeInto(LibraryManager.library, {
    cpp_main_call_js: function(){
        console.log(">>> cpp_main_call_js");
        return 0;
    },

    js_console_log_int: function(param) {
        console.log(">>> js_console_log_int:" + param);
    }
})