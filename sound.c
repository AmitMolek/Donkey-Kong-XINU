

void playing_sound_controller (){

	asm{
	// make a sound
	in al, 61h
	or al, 00000011b // 111011
	out 61h, al
	mov al, 0b6h
	out 43h, al
	mov ax, 2394h
	out 42h, al
	mov al, ah
	out 42h, al
	}
}

void stopping_sound_controller (){
	
	asm{
	// stop sound
	in al, 61h
	and al, 11111100b
	out 61h, a
	}
}

void color_controller(){
	
	asm{	
	mov ah, 9
	mov bl, 9 // number of the color
	mov cx, 11 //number of chars that in the text
	int 10h
	mov DX,OFFSET TXT
	int 
	}
}
