#include "core/app.hpp"

int main(){
    App app;
    app_init(app);
    app_run(app);
    app_shutdown(app);
    return 0;
}