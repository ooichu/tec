(= Monster (class
  :dir 1
 
  :step (func ()
    ; move
    (if (super':smack?) (= :dir (* :dir -1)))
    (super':move (* :dir 0.1) 0)
    ; update
    (super':step)
    ; draw
    (draw (super':get-x) (super':get-y) monster.bmp
      (+ (if (< :dir 0) 4 0)
         (% (* 4 (time)) 4)
      )
    )
  )
 
  init (func (x y)
    (= super (Enemy self x y 8))
  )
))
