MOD_ALT = 8
KEY_ENTER = 0xff0d

function open_menu()
    spawn({"dmenu_run"});
end

keys = {
    {MOD_ALT, KEY_ENTER, open_menu}
}
