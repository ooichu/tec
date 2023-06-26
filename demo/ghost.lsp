(= Ghost (class
  :dir 0

  :step (func ()
    (if player (do
      (let dist (distance player super))
      (if (< 8 dist 64)
        (super':move
          (/ (- (player':get-x) (super':get-x)) dist 10)
          (/ (- (player':get-y) (super':get-y)) dist 10)
        )
      )
      (= :dir (if (< (player':get-x) (super':get-x)) 2 0))
    ))
    (super':step)
    (draw (super':get-x) (super':get-y) ghost.bmp
      (+ :dir (% (* 4 (time)) 2))
    )
  )

  init (func (x y)
    (= super (Enemy self x y 8 t))
  )
))
