# ~/.bashrc — Kian OS (user `kian`)
# Kian Terminal look: green user@host, blue path (matches the classic prompt).

[[ $- != *i* ]] && return

alias ls='ls --color=auto'
alias ll='ls -alF --color=auto'
alias grep='grep --color=auto'
alias update='sudo pacman -Syu'
alias install='sudo pacman -S'

parse_git_branch() {
    git branch 2>/dev/null | sed -n 's/^\* \(.*\)/ (\1)/p'
}

PS1='\[\e[32m\]\u@\kian-os\[\e[0m\]:\[\e[34m\]\w\[\e[0m\]\[\e[33m\]$(parse_git_branch)\[\e[0m\] \$ '

export EDITOR=nano
export VISUAL=nano

# Real Bash completion (commands, paths, pacman, git, ...)
if [ -f /usr/share/bash-completion/bash_completion ]; then
    . /usr/share/bash-completion/bash_completion
fi
