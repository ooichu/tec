(= Pufft (class
  :step (func ()
    (super':step)
    (fill 15 (super':get-x) (super':get-y) 2 2) 
  )
  
  init (func (x y a)
    (= super (Particle self x y a 8))
  )
))

(= make-puffts (func (x y n)
  (each n (func (i)
    (Pufft x y (* i (/ (* 2 PI) n)))
  ))
))
