require "keysyms"

function open_menu()
    spawn({"dmenu_run"});
end

keys = {
    {MOD_ALT, KEY_p, open_menu},
}
