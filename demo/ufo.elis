(= Ufo (class
  :dir 1
 
  :step (func ()
    ; move
    (if (super':smack?) (= :dir (* :dir -1)))
    (super':move (* :dir 0.1) (* GRAVITY -1))
    ; update
    (super':step)
    ; draw
    (draw (super':get-x) (super':get-y) ufo.bmp
      (% (* 4 (time)) 3)
    )
  )
 
  init (func (x y)
    (= super (Enemy self x y 8))
  )
))
